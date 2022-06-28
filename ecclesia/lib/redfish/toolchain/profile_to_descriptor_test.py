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
"""Tests for profile_to_csdl."""

import json
import os

from absl.testing import absltest

from google.protobuf import text_format
from ecclesia.lib.protobuf import compare
from ecclesia.lib.redfish.toolchain import descriptor_pb2
from ecclesia.lib.redfish.toolchain import profile_to_descriptor


class ProfileToDescriptorTest(absltest.TestCase):

  def test_produces_expected_output(self):
    """Run the profile_to_csdl function and check output against test/csdl_out."""
    workingdir = os.path.dirname(__file__)
    input_files = [
        os.path.join(workingdir, 'test/csdl_in', f)
        for f in os.listdir(os.path.join(workingdir, 'test/csdl_in'))
    ]

    input_profile = None
    input_filepath = os.path.join(workingdir, 'test/simple_profile.json')
    with open(input_filepath, 'r') as profile_file:
      input_profile = json.load(profile_file)
    if not input_profile:
      self.fail(f'Unable to load profile from {input_filepath}')

    actual_profile_proto = profile_to_descriptor.profile_to_descriptor(
        input_profile, input_files)

    # Load expected profile proto
    expected_profile_filepath = os.path.join(
        workingdir, 'test/simple_profile_descriptor.textproto')
    expected_profile_proto = None
    with open(expected_profile_filepath, 'r') as expected_profile_file:
      expected_profile_proto = text_format.ParseLines(expected_profile_file,
                                                      descriptor_pb2.Profile())
    if not expected_profile_proto:
      self.fail('Failed to load profile from ' + expected_profile_filepath)

    compare.assert_proto_messages_equal(
        self,
        expected_profile_proto,
        actual_profile_proto,
        ignore_repeated_fields_order=True)


if __name__ == '__main__':
  absltest.main()
