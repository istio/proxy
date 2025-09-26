import sys
import unittest

from another_proto import message_pb2


class TestCase(unittest.TestCase):
    def test_message(self):
        got = message_pb2.TestMessage(
            index=5,
        )
        self.assertIsNotNone(got)


if __name__ == "__main__":
    sys.exit(unittest.main())
