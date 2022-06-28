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
"""Tests for comparing protobuffers in Python."""

from absl.testing import absltest

from ecclesia.lib.protobuf import compare
from ecclesia.lib.protobuf import compare_test_pb2


class CompareTest(absltest.TestCase):

  def test_simple_field_checks(self):
    proto1 = compare_test_pb2.TestProto(field1="1", field2=["1", "2", "3"])
    # Should be equal to self
    compare.assert_proto_messages_equal(self, proto1, proto1)
    proto2 = compare_test_pb2.TestProto(field1="1", field2=["2", "1", "3"])
    compare.assert_proto_messages_equal(
        self, proto1, proto2, ignore_repeated_fields_order=True)

  def test_nested_messages(self):
    proto1 = compare_test_pb2.TestProto(
        nested_test1=compare_test_pb2.TestProto.NestedTest(
            nested_field1=1, nested_field2=[2, 3, 4]))
    proto2 = compare_test_pb2.TestProto(
        nested_test1=compare_test_pb2.TestProto.NestedTest(
            nested_field1=1, nested_field2=[4, 3, 2]))
    compare.assert_proto_messages_equal(
        self, proto1, proto2, ignore_repeated_fields_order=True)

  def test_repeated_messages(self):
    proto1 = compare_test_pb2.TestProto(nested_test2=[
        compare_test_pb2.TestProto.NestedTest(nested_field1=1),
        compare_test_pb2.TestProto.NestedTest(nested_field2=[4, 3, 2]),
        compare_test_pb2.TestProto.NestedTest(
            nested_field1=1, nested_field2=[1, 1, 3])
    ])
    proto2 = compare_test_pb2.TestProto(nested_test2=[
        compare_test_pb2.TestProto.NestedTest(nested_field2=[2, 3, 4]),
        compare_test_pb2.TestProto.NestedTest(
            nested_field1=1, nested_field2=[3, 1, 1]),
        compare_test_pb2.TestProto.NestedTest(nested_field1=1)
    ])
    compare.assert_proto_messages_equal(
        self, proto1, proto2, ignore_repeated_fields_order=True)

  def test_simple_map_messages(self):
    proto1 = compare_test_pb2.TestProto(map_field1={"1": "1", "2": "2"})
    proto2 = compare_test_pb2.TestProto(map_field1={"2": "2", "1": "1"})
    compare.assert_proto_messages_equal(
        self, proto1, proto2, ignore_repeated_fields_order=True)

  def test_nested_map_messages(self):
    proto1 = compare_test_pb2.TestProto(
        map_field2={
            "1":
                compare_test_pb2.TestProto.NestedTest(nested_field2=[2, 3, 4]),
            "2":
                compare_test_pb2.TestProto.NestedTest(
                    nested_field1=1, nested_field2=[1, 3, 1])
        })
    proto2 = compare_test_pb2.TestProto(
        map_field2={
            "2":
                compare_test_pb2.TestProto.NestedTest(
                    nested_field1=1, nested_field2=[3, 1, 1]),
            "1":
                compare_test_pb2.TestProto.NestedTest(nested_field2=[2, 4, 3])
        })
    compare.assert_proto_messages_equal(
        self, proto1, proto2, ignore_repeated_fields_order=True)

  def test_failed_assertions_with_nested_fields(self):
    proto1 = compare_test_pb2.TestProto(nested_test2=[
        compare_test_pb2.TestProto.NestedTest(nested_field1=1),
        compare_test_pb2.TestProto.NestedTest(nested_field2=[4, 3, 2]),
        compare_test_pb2.TestProto.NestedTest(
            nested_field1=1, nested_field2=[1, 1, 3])
    ])
    proto2 = compare_test_pb2.TestProto(nested_test2=[
        compare_test_pb2.TestProto.NestedTest(nested_field2=[2, 3, 4]),
        compare_test_pb2.TestProto.NestedTest(
            nested_field1=1, nested_field2=[3, 1, 1]),
        compare_test_pb2.TestProto.NestedTest(nested_field1=1)
    ])
    self.assertRaises(
        AssertionError,
        lambda: compare.assert_proto_messages_equal(self, proto1, proto2))


if __name__ == "__main__":
  absltest.main()
