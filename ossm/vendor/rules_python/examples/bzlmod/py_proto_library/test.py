import json
import unittest

from proto import pricetag_pb2


class TestCase(unittest.TestCase):
    def test_pricetag(self):
        got = pricetag_pb2.PriceTag(
            name="dollar",
            cost=5.00,
        )

        metadata = {"description": "some text..."}
        got.metadata.value = json.dumps(metadata).encode("utf-8")

        self.assertIsNotNone(got)


if __name__ == "__main__":
    unittest.main()
