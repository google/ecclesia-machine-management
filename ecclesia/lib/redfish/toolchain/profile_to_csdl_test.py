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

import filecmp
import os
from xml.etree import ElementTree

from absl.testing import absltest
from ecclesia.lib.redfish.toolchain import profile_to_csdl


class ProfileToCsdlTest(absltest.TestCase):

  def test_produces_expected_output(self):
    """Run the profile_to_csdl function and check output against test/csdl_out."""
    workingdir = os.path.dirname(__file__)
    input_files = [
        os.path.join(workingdir, 'test/csdl_in', f)
        for f in os.listdir(os.path.join(workingdir, 'test/csdl_in'))
    ]

    actual_tmpdir = self.create_tempdir()
    profile_to_csdl.profile_to_csdl(
        os.path.join(workingdir, 'test/simple_profile.json'), input_files,
        actual_tmpdir.full_path)

    # Account for differences in the XML parsing library by reading in all
    # the test/csdl_out files and outputting them back out without changes.
    expected_tmpdir = self.create_tempdir()
    for f in os.listdir(os.path.join(workingdir, 'test/csdl_out')):
      if os.path.isfile(f):
        tree = ElementTree.parse(f)
        tree.write('%s/%s' % (expected_tmpdir.full_path, os.path.basename(f)))

    comp = filecmp.dircmp(expected_tmpdir.full_path, actual_tmpdir.full_path)
    self.assertEmpty(comp.diff_files)


if __name__ == '__main__':
  absltest.main()
