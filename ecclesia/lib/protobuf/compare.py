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
"""Library for handling comparisons between protobuffer messages in Python."""

from absl.testing import absltest

from google.protobuf import descriptor
from google.protobuf import message


def _key_for_message(proto_message: message.Message) -> bytes:
  """Helper function to provide a key for sorting Messages."""
  return proto_message.SerializeToString(deterministic=True)


def assert_proto_messages_equal(
    self: absltest.TestCase,
    a: message.Message,
    b: message.Message,
    ignore_repeated_fields_order: bool = False) -> None:
  """Compare two protobuffer messages and assert they're the same.

  Args:
    self: Test case that is calling this assert.
    a: Message to compare.
    b: Message to compare.
    ignore_repeated_fields_order: field to describe whether to ignore ordering
      in repeated fields when comparing the Messages.  In the case where
      ignore_repeated_fields is set to True, the function goes through all of
      the nested repeated fields within the messages provided and sorts them
      before comparing the protos.
  """
  if ignore_repeated_fields_order:
    for pb in a, b:
      queue: list[message.Message] = []
      queue.append(pb)
      while queue:
        current_message = queue.pop(0)
        # For each field in the message, if it's repeated, sort it
        # If there are nested messages available, add them to the queue
        for field, value in current_message.ListFields():
          if field.label == descriptor.FieldDescriptor.LABEL_REPEATED:
            # Variable to determine which values to add to the queue
            iterate_through_value = value
            # Field is either repeated Message or map
            if field.message_type:
              if field.message_type.GetOptions().map_entry:
                # Field is map; iterator needs to point the values
                # Note that maps cannot be sorted but
                # deterministic serialization handles this problem
                iterate_through_value = value.values()
              else:
                # Repeated field of Messages; sort it using special key
                value.sort(key=_key_for_message)
            else:
              # The iterator isn't Message based so sort normally
              value.sort()
            for v in iterate_through_value:
              if not isinstance(v, message.Message):
                continue
              queue.append(v)
          elif field.type == descriptor.FieldDescriptor.TYPE_MESSAGE:
            queue.append(value)

  self.assertEqual(
      a.SerializeToString(deterministic=True),
      b.SerializeToString(deterministic=True))
