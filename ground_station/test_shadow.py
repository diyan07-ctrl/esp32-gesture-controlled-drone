import binascii
import math
import struct
import unittest
from .shadow_codec import decode, SIZE


def fixture(mode=1):
    b = bytearray(SIZE)
    b[:3] = b"PS\x01"
    b[3] = mode
    struct.pack_into("<H", b, 4, SIZE)
    struct.pack_into("<4I", b, 8, 42, 1000, 4000, 0)
    struct.pack_into("<9f", b, 24, 5, 4, 1, .01, .001, -.002, .009, 4, 2)
    if mode == 1:
        struct.pack_into("<4f", b, 132, .3, .4, .4, .3)
    struct.pack_into("<4f", b, 172, 1, 0, 0, 0)
    b[188] = 0x3c
    b[189] = 1
    struct.pack_into("<H", b, 192, binascii.crc_hqx(b[:192],0xffff))
    return b


class ShadowTests(unittest.TestCase):
    def test_decode_and_physical_output_separation(self):
        row = decode(fixture())
        self.assertEqual(row["axes"]["roll"]["p"], struct.unpack("<f", struct.pack("<f",.01))[0])
        self.assertEqual(row["sequence"],42)
        self.assertFalse(row["esc_output_enabled"])
        self.assertIsNone(row["esc_pwm_us"])
        self.assertIsNone(row["actual_motor_operation"])

    def test_reject_corruption_nonfinite_and_output_flag(self):
        for offset in (2, 40, 190):
            data = fixture()
            data[offset] ^= 1
            with self.assertRaises(ValueError):decode(data)
        data = fixture()
        struct.pack_into("<f",data,132,math.nan)
        struct.pack_into("<H",data,192,binascii.crc_hqx(data[:192],0xffff))
        with self.assertRaises(ValueError):decode(data)
        with self.assertRaises(ValueError):decode(fixture()[:-1])

    def test_motor_locked_cannot_report_calculated_motor(self):
        row = decode(fixture(mode=2))
        self.assertEqual(row["calculated_motor"],(0,0,0,0))
        data = fixture(mode=2)
        struct.pack_into("<f",data,132,.1)
        struct.pack_into("<H",data,192,binascii.crc_hqx(data[:192],0xffff))
        with self.assertRaises(ValueError):decode(data)


if __name__ == "__main__": unittest.main()
