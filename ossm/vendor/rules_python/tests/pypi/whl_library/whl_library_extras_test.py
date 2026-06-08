import unittest


class NamespacePackagesTest(unittest.TestCase):

    def test_extras_propagated(self):
        import pkg

        self.assertEqual(pkg.WITH_EXTRAS, True)


if __name__ == "__main__":
    unittest.main()
