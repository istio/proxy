"Test for circllhist"

import unittest
from circllhist import Circllhist, Circllbin

class TestHistogram(unittest.TestCase):

    def test_hist(self):
        h = Circllhist()
        h.insert(123,3)
        h.insert_intscale(1,1)
        self.assertEqual(h.count(), 4)
        self.assertEqual(h.bin_count(), 2)
        self.assertAlmostEqual(h.sum(), 385.5)
        self.assertAlmostEqual(h.mean(), 96.375)
        self.assertAlmostEqual(h.quantile(0.5), 123.333, 1)
        self.assertTrue(str(h))
        g = Circllhist.from_dict(h.to_dict())
        self.assertEqual(h.sum(), g.sum())
        h.merge(g)
        f = Circllhist.from_b64(h.to_b64())
        self.assertEqual(h.sum(), f.sum())
        h.clear()
        self.assertEqual(h.count(), 0)

    def test_bin(self):
        b = Circllbin.from_number(123.3)
        self.assertEqual(b.width,10)
        self.assertEqual(b.midpoint,125)
        self.assertEqual(b.edge,120)

if __name__ == '__main__':
    unittest.main(verbosity=2)
