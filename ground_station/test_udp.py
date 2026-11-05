import binascii
import socket
import struct
import unittest
from .telemetry_codec import decode, TelemetryError, SIZE
from .udp_listener import Tracker


def fixture(sequence=1,uptime=4000,flags=0):
    data=bytearray(SIZE)
    data[:4]=b"FT\x02\x02"
    struct.pack_into("<HHII4B",data,4,SIZE,flags,sequence,uptime,4,1,0,0)
    struct.pack_into("<12I",data,20,4000,100,150,0,0,0,0,0,0,0,0,0xffffffff)
    struct.pack_into("<HhhhHH",data,68,0,0,0,0,0,0)
    struct.pack_into("<3f",data,80,0,0,1)
    struct.pack_into("<3f",data,92,0,0,0)
    struct.pack_into("<3f",data,104,0,0,0)
    struct.pack_into("<4f",data,120,1,0,0,0)
    struct.pack_into("<H",data,140,binascii.crc_hqx(data[:140],0xffff))
    return bytes(data)


class UdpTests(unittest.TestCase):
    def test_packet_decode_and_unavailable_fields(self):
        result=decode(fixture())
        self.assertEqual(result["state"],"WAITING_FOR_GLOVE")
        self.assertEqual(result["sequence"],1)
        self.assertIsNone(result["roll_deg"])
        self.assertIsNone(result["throttle"])
        self.assertIsNone(result["command_age_ms"])
        result=decode(fixture(flags=4|512))
        self.assertEqual(result["accel_g"],(0,0,1))
        self.assertEqual(result["quaternion_sensor_frame"],(1,0,0,0))

    def test_reject_corruption_length_version_ranges_and_output_flag(self):
        for broken in [fixture()[:-1],b"x"+fixture()[1:],
                       bytearray(fixture())]:
            if isinstance(broken,bytearray):broken[42]^=1
            with self.assertRaises(TelemetryError):decode(broken)
        for offset,value in [(2,1),(3,1),(16,9),(17,15),(68,1001),(6,128)]:
            data=bytearray(fixture())
            if offset==68:struct.pack_into("<H",data,offset,value)
            else:data[offset]=value
            struct.pack_into("<H",data,140,binascii.crc_hqx(data[:140],0xffff))
            with self.assertRaises(TelemetryError):decode(data)

    def test_tracker_gap_reorder_reboot_and_stale(self):
        tracker=Tracker(.2)
        self.assertEqual(tracker.condition(0),"NO_PACKET")
        tracker.ingest(fixture(1),1)
        tracker.ingest(fixture(4,4020),1.02)
        self.assertEqual(tracker.missing,2)
        self.assertEqual(tracker.condition(1.3),"STALE_TELEMETRY_OR_NETWORK_LOSS")
        with self.assertRaises(TelemetryError):tracker.ingest(fixture(4,4020),1.04)
        self.assertEqual(tracker.invalid,1)
        tracker.ingest(fixture(0,100),2)
        self.assertEqual(tracker.missing,2)
        self.assertEqual(tracker.valid,3)

    def test_actual_udp_loopback_socket(self):
        receiver=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        receiver.bind(("127.0.0.1",0));receiver.settimeout(1)
        sender=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        try:
            sender.sendto(fixture(7),("127.0.0.1",receiver.getsockname()[1]))
            data,_=receiver.recvfrom(512)
            self.assertEqual(decode(data)["sequence"],7)
        finally:receiver.close();sender.close()


if __name__=="__main__":unittest.main()
