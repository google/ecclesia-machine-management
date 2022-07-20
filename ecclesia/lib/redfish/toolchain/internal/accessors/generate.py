"""Utility to generate Redfish accessor classes.

This binary does not accept any command line arguments. The accessors are
generated based entirely upon embedded data.

"""
import argparse
import sys
from typing import Sequence

import jinja2

from ecclesia.lib.jinja2 import loader
from ecclesia.lib.redfish.toolchain.internal import descriptor_pb2

# Directory relative to ecclesia/.
_PACKAGE_DIR = 'ecclesia/lib/redfish/toolchain/internal/accessors'

# Map of descriptor primitive types to their C++ equivalent.
_PRIMITIVE_TYPE_MAP = {
    descriptor_pb2.Property.Type.PrimitiveType.BOOLEAN: 'bool',
    descriptor_pb2.Property.Type.PrimitiveType.INT64: 'int64_t',
    descriptor_pb2.Property.Type.PrimitiveType.STRING: 'std::string',
    descriptor_pb2.Property.Type.PrimitiveType.DECIMAL: 'double',
    descriptor_pb2.Property.Type.PrimitiveType.DOUBLE: 'double',
    descriptor_pb2.Property.Type.PrimitiveType.DATE_TIME_OFFSET: 'absl::Time',
    descriptor_pb2.Property.Type.PrimitiveType.DURATION: 'absl::Duration',
    descriptor_pb2.Property.Type.PrimitiveType.GUID: 'uint64_t',
}


class ProfileDescriptor:
  """Wraps a descriptor protobuf and provides some common member transforms."""

  def __init__(self, pb: descriptor_pb2.Profile):
    self.pb = pb
    self.sanitized_profile_name = pb.profile_name.replace(' ', '')


def type_to_string(ptype: descriptor_pb2.Property.Type) -> str:
  """Converts a Property.Type to a C++ string.

  Args:
    ptype: property type to convert.

  Returns:
    string of the C++ type.

  Raises:
    NotImplementedError if the translation cannot be completed.

  """
  oneof = ptype.WhichOneof('type')
  if not oneof:
    return ''
  attr = getattr(ptype, oneof)

  if oneof == 'primitive':
    if attr in _PRIMITIVE_TYPE_MAP:
      return _PRIMITIVE_TYPE_MAP[attr]
    raise NotImplementedError(
        f'type_to_string() cannot map primitive type "{attr}" into a C++ type')
  elif isinstance(attr, type(ptype.reference)) or isinstance(
      attr, type(ptype.collection)):
    # trivial placeholder.
    return 'RedfishVariant'
  else:
    raise NotImplementedError(
        f'type_to_string() cannot map oneof "{oneof}" into a C++ type')


def main(argv: Sequence[str]) -> None:
  parser = argparse.ArgumentParser(
      description='Generate profile based accessors.')
  parser.add_argument(
      '--proto_path_in',
      type=str,
      help='filepath of the compiled proto file for input')
  parser.add_argument('--h_path', type=str, help='filepath of the .h file')
  parser.add_argument(
      '--h_include', type=str, help='filepath for including header file')
  parser.add_argument('--cc_path', type=str, help='filepath of the .cc file')
  args = parser.parse_args(argv[1:])

  with open(args.proto_path_in, 'rb') as f:
    pb = descriptor_pb2.Profile.FromString(f.read())

  render_dict = {
      'profiles': [ProfileDescriptor(pb)],
      'header_filepath': args.h_include
  }

  # Use the constructed environment to render the template.
  jinja_env = jinja2.Environment(
      loader=loader.ResourceLoader(_PACKAGE_DIR, 'ecclesia'),
      undefined=jinja2.StrictUndefined)
  jinja_env.globals['type_to_string'] = type_to_string

  with open(args.cc_path, 'w') as f:
    jinja_template = jinja_env.get_template('accessors.cc.jinja2')
    f.write(jinja_template.render(**render_dict))
  with open(args.h_path, 'w') as f:
    jinja_template = jinja_env.get_template('accessors.h.jinja2')
    f.write(jinja_template.render(**render_dict))


if __name__ == '__main__':
  main(sys.argv)
