# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Reduces a Redfish Schema to a minimum definition to satisfy a Profile."""

import json
import os
import re
from typing import Any, Mapping, Optional, Sequence, Tuple
from xml.etree import ElementTree


def _version_string_into_tuple(version_str: str) -> Tuple[int]:
  """Takes an input string like "1.2.3" and outputs a tuple like (1,2,3)."""
  return tuple(int(s) for s in version_str.split('.'))


class Requirement:
  """Data structure for the Redfish Profile requirements for one Resource.

  Attributes:
   properties: list of all required property names.
   min_version: optional tuple representing the minimum Resource version.
  """

  def __init__(self, properties: Sequence[str],
               min_version: Optional[Tuple[int]]):
    self.properties = properties
    self.min_version = min_version

  def __str__(self):
    version_str = '(%s)' % (self.min_version,) if self.min_version else ''
    properties = ', '.join(self.properties)
    return '%s : [%s]' % (version_str, properties)


def _process_profile_to_requirement_map(
    profile: Mapping[str, Any]) -> Mapping[str, Requirement]:
  """Parses a Profile file into Requirement map.

  Args:
    profile: JSON representation of a Redfish Profile.

  Returns:
    map from Resource name to Requirement.
  """
  all_profile_requirements = {}
  for resource, resource_reqs in profile['Resources'].items():
    min_version = None
    if 'MinVersion' in resource_reqs:
      min_version = resource_reqs['MinVersion']

    properties = []
    if 'PropertyRequirements' in resource_reqs:
      for prop in resource_reqs['PropertyRequirements'].keys():
        properties.append(prop)

    all_profile_requirements[resource] = Requirement(
        properties,
        _version_string_into_tuple(min_version) if min_version else None)
  return all_profile_requirements


def _delete_nonrequired_element(property_requirements: Requirement, parent: Any,
                                elem: Any) -> bool:
  """Removes elem from parent if it is not required.

  Args:
    property_requirements: Requirement.
    parent: parent element containing elem.
    elem: element to be evaluated for deletion.

  Returns:
    True if elem was removed from parent, False otherwise.
  """
  if elem.get('Name') not in property_requirements.properties:
    parent.remove(elem)
    return True
  return False


def _delete_nonrequired_properties(property_requirements: Requirement,
                                   parent: Any) -> None:
  """Removes properties from parent if they are not required.

  Args:
    property_requirements: Requirement.
    parent: parent element containing properties.
  """
  for prop in parent.findall(
      '{http://docs.oasis-open.org/odata/ns/edm}Property'):
    _delete_nonrequired_element(property_requirements, parent, prop)
  for prop in parent.findall(
      '{http://docs.oasis-open.org/odata/ns/edm}NavigationProperty'):
    _delete_nonrequired_element(property_requirements, parent, prop)


def _trim_schema_according_to_profile(
    tree: ElementTree.ElementTree,
    processed_profile_data: Mapping[str, Requirement]) -> bool:
  """Removes elements from schema if they are not required.

  Args:
    tree: ElementTree representation of the Redfish Resource schema.
    processed_profile_data: map from Resource name to Requirement.

  Returns:
    True if this schema contains any requirements from processed_profile_data,
    False otherwise.
  """
  root = tree.getroot()
  data_services = root.findall(
      '{http://docs.oasis-open.org/odata/ns/edmx}DataServices')
  has_any_resources = False

  for ds in data_services:
    schemas = ds.findall('{http://docs.oasis-open.org/odata/ns/edm}Schema')
    for schema in schemas:
      schema_namespace = schema.get('Namespace')
      if not schema_namespace:
        continue
      schema_ns_parts = schema_namespace.split('.')
      schema_name = schema_ns_parts[0]
      if schema_name not in processed_profile_data:
        ds.remove(schema)
        continue

      if len(schema_ns_parts) >= 2:
        version_tuple = _version_string_into_tuple(schema_ns_parts[1].replace(
            'v', '').replace('_', '.'))
        minimum_version = (1, 0, 0)
        if processed_profile_data[schema_name].min_version:
          minimum_version = processed_profile_data[schema_name].min_version
        if version_tuple > minimum_version:
          ds.remove(schema)
          continue

      has_any_resources = True

      entity_types = schema.findall(
          '{http://docs.oasis-open.org/odata/ns/edm}EntityType')
      for entity_type in entity_types:
        _delete_nonrequired_properties(processed_profile_data[schema_name],
                                       entity_type)

      complex_types = schema.findall(
          '{http://docs.oasis-open.org/odata/ns/edm}ComplexType')
      for complex_type in complex_types:
        if _delete_nonrequired_element(processed_profile_data[schema_name],
                                       schema, complex_type):
          continue
        _delete_nonrequired_properties(processed_profile_data[schema_name],
                                       complex_type)

      # TODO(b/228641689): Support actions.
      actions = schema.findall(
          '{http://docs.oasis-open.org/odata/ns/edm}Action')
      for action in actions:
        schema.remove(action)

  return has_any_resources


def _lint_dependencies(tree: ElementTree.ElementTree):
  """Cleans up dependencies in a Redfish Schema.

  Removes non-referenced imports and type definitions.

  Args:
    tree: ElementTree representation of the Redfish Resource schema.
  """
  root = tree.getroot()
  data_services = root.findall(
      '{http://docs.oasis-open.org/odata/ns/edmx}DataServices')
  # Find all dependencies from attributes.
  dependencies = []
  for ds in data_services:
    for entity in ds.iter(
        '{http://docs.oasis-open.org/odata/ns/edm}EntityType'):
      dependencies.append(entity.get('BaseType'))
    for annotation in ds.iter(
        '{http://docs.oasis-open.org/odata/ns/edm}Annotation'):
      dependencies.append(annotation.get('Term'))
    for prop in ds.iter('{http://docs.oasis-open.org/odata/ns/edm}Property'):
      dependencies.append(prop.get('Type'))
    for prop in ds.iter(
        '{http://docs.oasis-open.org/odata/ns/edm}NavigationProperty'):
      dependencies.append(prop.get('Type'))
  dependency_dict = {}
  for dependency in dependencies:
    # Use regexp to handle special case for "Collection(ENTITY.TYPE)"
    parameterised_type = re.search(r'[a-zA-Z\.]+\(([a-zA-Z\.]+)\)', dependency)
    if parameterised_type:
      dependency = parameterised_type.group(1)
    if not dependency:
      continue
    # rpartition returns a 3-tuple (prefix, delimiter, suffix).
    # On failure, it returns a 3-tuple ('', '', original_str).
    prefix, delim, suffix = dependency.rpartition('.')
    # Expect all dependencies to be PACKAGE.ENTITY or PACKAGE.VERSION.ENTITY.
    if not delim:
      continue
    if prefix not in dependency_dict:
      dependency_dict[prefix] = set()
    dependency_dict[prefix].add(suffix)

  # Iterate over all References and delete unnecessary includes.
  for reference in root.findall(
      '{http://docs.oasis-open.org/odata/ns/edmx}Reference'):
    has_dependent_includes = False
    for include in reference.findall(
        '{http://docs.oasis-open.org/odata/ns/edmx}Include'):
      if include.get('Namespace') in dependency_dict or include.get(
          'Alias') in dependency_dict:
        has_dependent_includes = True
      else:
        reference.remove(include)
    if not has_dependent_includes:
      root.remove(reference)

  # Iterate over all EnumType and delete unnecessary definitions.
  # This may delete an EnumType incorrectly as references across Schemas are
  # currently not tracked.
  # TODO(b/228641689): Support cross-schema references.
  for ds in data_services:
    for schema in ds.findall('{http://docs.oasis-open.org/odata/ns/edm}Schema'):
      schema_namespace = schema.get('Namespace')
      for enum in schema.findall(
          '{http://docs.oasis-open.org/odata/ns/edm}EnumType'):
        if schema_namespace not in dependency_dict or enum.get(
            'Name') not in dependency_dict[schema_namespace]:
          schema.remove(enum)


def profile_to_csdl(profile_file: str, schema_files: Sequence[str],
                    output_dir: str) -> None:
  """Produces a trimmed CSDL definition based on the provided Profile.

  Args:
    profile_file: filepath of the Redfish Profile.
    schema_files: sequence containing all schema CSDL files.
    output_dir: dirpath to write the trimmed CSDL files.

  Raises:
    RuntimeError: on failure to load Redfish Profile.
  """
  # Register namespaces used in Redfish XML schemas.
  ElementTree.register_namespace('', 'http://docs.oasis-open.org/odata/ns/edm')
  ElementTree.register_namespace('edmx',
                                 'http://docs.oasis-open.org/odata/ns/edmx')

  # Process the Redfish Profile from file into a requirement map.
  processed_profile_data = None
  with open(profile_file, 'r') as profile_f:
    profile_data = json.load(profile_f)
    processed_profile_data = _process_profile_to_requirement_map(profile_data)
  if not processed_profile_data:
    raise RuntimeError('Could not load provided profile.')

  # Iterate over the entire Redfish schema and remove elements not required
  # by the Redfish Profile. Output the trimmed schema as a new file.
  for filename in schema_files:
    if os.path.isfile(filename):
      tree = ElementTree.parse(filename)
      if _trim_schema_according_to_profile(tree, processed_profile_data):
        _lint_dependencies(tree)
        tree.write('%s/%s' % (output_dir, os.path.basename(filename)))

