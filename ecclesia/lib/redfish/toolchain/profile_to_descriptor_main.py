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
"""Runner for the profile_to_descriptor library."""

import argparse
import json

from google.protobuf import text_format
from ecclesia.lib.redfish.toolchain import profile_to_descriptor


def main() -> None:
  parser = argparse.ArgumentParser(
      description='Converts the provided Redfish profile and CSDL information '
      + 'into a descriptor proto and writes the proto into the output ' +
      'file location')
  parser.add_argument(
      '--csdl_files',
      type=str,
      nargs='*',
      required=True,
      help='relative dirpath of CSDL files')
  parser.add_argument(
      '--profile', type=str, required=True, help='profile filepath')
  parser.add_argument(
      '--output_file',
      type=str,
      required=True,
      help='output file for binary descriptor proto')
  parser.add_argument(
      '--debug_output',
      type=str,
      required=False,
      help='file to output text proto format of descriptor proto')

  args = parser.parse_args()

  profile_data = {}
  with open(args.profile) as profile_file:
    profile_data = json.load(profile_file)

  profile_proto = profile_to_descriptor.profile_to_descriptor(
      profile_data, args.csdl_files)

  with open(args.output_file, 'wb+') as output_file:
    output_file.write(profile_proto.SerializeToString())

  if args.debug_output:
    with open(args.debug_output, 'w+') as debug_file:
      debug_file.write(text_format.MessageToString(profile_proto))


if __name__ == '__main__':
  main()
