import math
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from battery_calculator import calculate as battery
from voltage_divider_calculator import calculate as divider
from telemetry_parser import records, FIELDS


class ToolsTest(unittest.TestCase):
    def test_battery(self):
        result = battery(11.1, 2200, 20)
        self.assertAlmostEqual(result["nominal_Wh"], 24.42)
        self.assertIsNone(result["label_continuous_A"])
        self.assertEqual(battery(11.1, 2200, 20, c_rating=35)["label_continuous_A"], 77)

    def test_invalid(self):
        for value in (0, -1, math.nan, math.inf):
            with self.assertRaises(ValueError):
                battery(11.1, 2200, value)

    def test_divider(self):
        result = divider()
        self.assertAlmostEqual(result["adc_V"], 2.2105263158)
        self.assertLess(result["worst_high_adc_V"], 2.25)
        self.assertAlmostEqual(result["divider_A"], 12.6/57000)

    def test_telemetry(self):
        self.assertEqual(len(list(records(["D," + ",".join(["0"] * len(FIELDS))]))), 1)
        with self.assertRaises(ValueError):
            list(records(["D,1"]))


if __name__ == "__main__":
    unittest.main()
