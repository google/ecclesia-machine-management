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
"""Constructs a Descriptor proto based on a Redfish Profile and CSDL.

The library constructs the proto by reducing the definition of the CSDL for each
component so that just the used types are described.
"""

import os
from typing import Sequence, Mapping, MutableMapping, DefaultDict, Optional, Tuple, MutableSet, Any
from xml.etree import ElementTree
from absl import logging

from ecclesia.lib.redfish.toolchain.internal import descriptor_pb2

# Constants used for parsing CSDL schema files
_CSDL_NAMESPACE_REGISTRY = {
    'edm': 'http://docs.oasis-open.org/odata/ns/edm',
    'edmx': 'http://docs.oasis-open.org/odata/ns/edmx'
}
# Tags for parsing the tree
_ODATA_TAG_DATA_SERVICES = 'edmx:DataServices'
_ODATA_TAG_SCHEMA = 'edm:Schema'
_ODATA_TAG_ENTITY_TYPE = 'edm:EntityType'
_ODATA_TAG_ENUM_TYPE = 'edm:EnumType'
_ODATA_TAG_PROPERTY = 'edm:Property'
_ODATA_TAG_NAVIGATION_PROPERTY = 'edm:NavigationProperty'
_ODATA_TAG_MEMBER = 'edm:Member'
# Common attributes of the elements in the CSDL
_ODATA_ATTRIBUTE_NAMESPACE = 'Namespace'
_ODATA_ATTRIBUTE_NAME = 'Name'
_ODATA_ATTRIBUTE_TYPE = 'Type'
# Map string EDM defined types to descriptor primitive type equivalent.
_PRIMITIVE_TYPE_MAP = {
    'Boolean': descriptor_pb2.Property.Type.BOOLEAN,
    'DateTimeOffset': descriptor_pb2.Property.Type.DATE_TIME_OFFSET,
    'Decimal': descriptor_pb2.Property.Type.DECIMAL,
    'Double': descriptor_pb2.Property.Type.DOUBLE,
    'Duration': descriptor_pb2.Property.Type.DURATION,
    'Guid': descriptor_pb2.Property.Type.GUID,
    'Int64': descriptor_pb2.Property.Type.INT64,
    'String': descriptor_pb2.Property.Type.STRING
}


def _remove_prefix(string: str, prefix: str) -> str:
  """Remove prefix from string and return the result string."""
  if string.startswith(prefix):
    return string[len(prefix):]
  return string


def _remove_suffix(string: str, suffix: str) -> str:
  """Remove suffix from string and return the result string."""
  if string.endswith(suffix):
    return string[:-len(suffix)]
  return string


class SchemaDefinition:
  """Schema definition class to store information about schema namespaces.

  Attributes:
   property_types: Map of entity and property name to the descriptor type.
   enums: Map of enum name to the descriptor Enum type.
  """

  def __init__(self):
    self.property_types: MutableMapping[str, MutableMapping[
        str, descriptor_pb2.Property.Type]] = DefaultDict(DefaultDict)
    self.enums: MutableMapping[str, descriptor_pb2.EnumType] = DefaultDict()

  def add_property_to_property_types(self, entity_name: str,
                                     property_name: Optional[str],
                                     property_type: Optional[str]) -> None:
    """Add a property to the mapping of entity and property name to a proto type."""
    if not property_name or not property_type:
      logging.warning('No property name/type for entity %s', entity_name)
      return
    descriptor_type = descriptor_pb2.Property.Type()
    # Parse the property type string
    # Case 1: prop_type is a primitive type ("Edm.<Type>")
    if property_type.startswith('Edm.'):
      primitive_type = _remove_prefix(property_type, 'Edm.')
      if primitive_type in _PRIMITIVE_TYPE_MAP:
        descriptor_type.primitive = _PRIMITIVE_TYPE_MAP[primitive_type]
      else:
        logging.warning('Unrecognized type for property %s: %s', property_name,
                        property_type)
    else:
      # Case 2: Property type is a reference or collection reference
      # i.e. type is "Collection(...)" or "<Namespace>{.<ver>}.<Entity>"
      reference_type: Optional[descriptor_pb2.Property.Reference] = None
      if property_type.startswith('Collection('):
        # Set reference type to the collection in the descriptor
        reference_type = descriptor_type.collection
        # Strip collection and parse reference same as normal reference
        property_type = _remove_suffix(
            _remove_prefix(property_type, 'Collection('), ')')
      else:
        reference_type = descriptor_type.reference

      # Property type should be 2 or 3 parts
      # Use first and last parts to get referenced type
      property_type_parts = property_type.split('.')
      if len(property_type_parts) == 2 or len(property_type_parts) == 3:
        reference_type.schema_namespace = property_type_parts[0]
        reference_type.entity_name = property_type_parts[-1]
      else:
        logging.warning('Unrecognized reference type for property %s: %s',
                        property_name, property_type)

    logging.info('Adding property %s to entity %s in schema definition',
                 property_name, entity_name)
    self.property_types[entity_name][property_name] = descriptor_type

  def process_schema_data(self, schema_element: ElementTree.Element) -> None:
    """Add the schema elements to the property and enums in this definition."""
    for entity_type in schema_element.findall(_ODATA_TAG_ENTITY_TYPE,
                                              _CSDL_NAMESPACE_REGISTRY):
      entity_name = entity_type.get(_ODATA_ATTRIBUTE_NAME)
      if not entity_name:
        logging.warning('Unable to find name for entity in namespace')
        continue
      for entity_property in entity_type.findall(_ODATA_TAG_PROPERTY,
                                                 _CSDL_NAMESPACE_REGISTRY):
        self.add_property_to_property_types(
            entity_name, entity_property.get(_ODATA_ATTRIBUTE_NAME),
            entity_property.get(_ODATA_ATTRIBUTE_TYPE))
      for entity_property in entity_type.findall(_ODATA_TAG_NAVIGATION_PROPERTY,
                                                 _CSDL_NAMESPACE_REGISTRY):
        self.add_property_to_property_types(
            entity_name, entity_property.get(_ODATA_ATTRIBUTE_NAME),
            entity_property.get(_ODATA_ATTRIBUTE_TYPE))

    for enum_type in schema_element.findall(_ODATA_TAG_ENUM_TYPE,
                                            _CSDL_NAMESPACE_REGISTRY):
      enum_name = enum_type.get(_ODATA_ATTRIBUTE_NAME)
      if not enum_name:
        logging.warning('Unable to find name for enum in namespace')
        continue

      proto_enum = descriptor_pb2.EnumType(entity_name=enum_name)

      for enum_member in enum_type.findall(_ODATA_TAG_MEMBER,
                                           _CSDL_NAMESPACE_REGISTRY):
        if enum_member.get(_ODATA_ATTRIBUTE_NAME):
          proto_enum.values.append(enum_member.get(_ODATA_ATTRIBUTE_NAME))

      logging.info('Adding enum %s to schema definition', enum_name)
      self.enums[enum_name] = proto_enum



def _preprocess_csdl_files(
    schema_files: Sequence[str]) -> Mapping[str, SchemaDefinition]:
  """Convert schema files into a mapping of namespace to SchemaDefinition."""
  namespace_mapping: MutableMapping[str, SchemaDefinition] = DefaultDict(
      SchemaDefinition)

  for filename in schema_files:
    if not os.path.isfile(filename):
      continue
    tree = ElementTree.parse(filename)
    for data_service in tree.findall(_ODATA_TAG_DATA_SERVICES,
                                     _CSDL_NAMESPACE_REGISTRY):
      # Extract all of the schema from the data service
      for schema in data_service.findall(_ODATA_TAG_SCHEMA,
                                         _CSDL_NAMESPACE_REGISTRY):
        schema_namespace = schema.get(_ODATA_ATTRIBUTE_NAMESPACE)
        if not schema_namespace:
          logging.warning('Schema found without namespace in %s', filename)
          continue

        # Strip namespace of versioning
        stripped_namespace = schema_namespace.split('.')[0]
        logging.info('Adding schema information for %s', stripped_namespace)
        namespace_mapping[stripped_namespace].process_schema_data(schema)

  return namespace_mapping


# builder utilities.
def profile_to_descriptor(
    profile_data: Mapping[str, Any],
    schema_files: Sequence[str]) -> descriptor_pb2.Profile:
  """Produces a proto descriptor of the schemas based on the provided Profile.

  Args:
    profile_data: Redfish Profile as JSON data.
    schema_files: sequence containing all schema CSDL files.

  Returns:
    Descriptor proto containing relevant properties and types.
  """
  schema_definitions_by_namespace = _preprocess_csdl_files(schema_files)


  profile_proto = descriptor_pb2.Profile(
      profile_name=profile_data['ProfileName'],
      profile_version=profile_data['ProfileVersion'],
      purpose=profile_data['Purpose'],
      contact_info=profile_data['ContactInfo'],
      owning_entity=profile_data['OwningEntity'])

  if 'Resources' in profile_data:
    # Track referenced properties to add in later
    referenced_properties: MutableSet[Tuple[str, str]] = set()
    for resource_name, resource_requirements in profile_data['Resources'].items(
    ):
      # Resources should be defined in their own namesake namespaces
      if resource_name not in schema_definitions_by_namespace:
        logging.warning('Resource lacks namespace definition: %s',
                        resource_name)
        continue

      # Go through required properties and add their types to the schema
      schema_definition = schema_definitions_by_namespace[resource_name]
      schema_proto = profile_proto.schemas_by_namespace[resource_name]
      schema_proto.schema_namespace = resource_name
      resource_proto = schema_proto.resources.add(entity_name=resource_name)
      for property_requirement in resource_requirements[
          'PropertyRequirements'].keys():
        property_proto = resource_proto.properties.add(
            name=property_requirement)
        if property_requirement not in schema_definition.property_types[
            resource_name]:
          logging.warning('Failed to find property %s in entity %s',
                          property_requirement, resource_name)
          continue
        property_proto.type.CopyFrom(
            schema_definition.property_types[resource_name]
            [property_requirement])

        # Add referenced properties via reference/collection to make sure they
        # exist
        if property_proto.type.HasField(
            'reference') or property_proto.type.HasField('collection'):
          reference_type = property_proto.type.reference if property_proto.type.HasField(
              'reference') else property_proto.type.collection
          referenced_properties.add(
              (reference_type.schema_namespace, reference_type.entity_name))

        # DSP0272_1.0.0 Section 11.2 ->
        # "One additional level of JSON objects may be embedded, essentially
        # nesting a 'PropertyRequirements' object."
        # The spec implies that at most 1 level of property requirement
        # nesting is allowed.
        # If the property is nested and is of type 'collection', then create a
        # new resource within the current schema and add properties per
        # namespace mapping.
        nested_property = resource_requirements['PropertyRequirements'][
            property_requirement].get('PropertyRequirements')
        if nested_property and property_proto.type.HasField('collection'):
          # Go through required properties and add their types to the schema
          nested_namespace = property_proto.type.collection.schema_namespace
          nested_entity = property_proto.type.collection.entity_name
          schema_definition = schema_definitions_by_namespace[nested_namespace]
          nested_resource_proto = schema_proto.resources.add(
              entity_name=nested_entity)

          for nested_requirement in nested_property.keys():
            property_proto = nested_resource_proto.properties.add(
                name=nested_requirement)
            if nested_requirement not in schema_definition.property_types[
                nested_entity]:
              logging.warning('Failed to find property %s in entity %s',
                              nested_requirement, nested_entity)
              continue
            property_proto.type.CopyFrom(
                schema_definition.property_types[nested_entity][
                    nested_requirement])

    # Add referenced types to the appropriate schema definition
    for referenced_prop_namespace, referenced_prop_entity in referenced_properties:
      if referenced_prop_namespace not in schema_definitions_by_namespace:
        logging.warning('Unknown namespace referenced: %s',
                        referenced_prop_namespace)
        continue
      if referenced_prop_entity in schema_definitions_by_namespace[
          referenced_prop_namespace].enums:
        # Add the enum to the schema proto
        profile_proto.schemas_by_namespace[
            referenced_prop_namespace].enums.append(
                schema_definitions_by_namespace[referenced_prop_namespace]
                .enums[referenced_prop_entity])
      elif referenced_prop_entity not in schema_definitions_by_namespace[
          referenced_prop_namespace].property_types:
        logging.warning('Entity %s in namespace %s never defined',
                        referenced_prop_entity, referenced_prop_namespace)

  return profile_proto
