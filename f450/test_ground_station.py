import json
import unittest
from unittest.mock import Mock
from f450_ground_station import Station


class GroundStationTests(unittest.TestCase):
    def test_numeric_rejection(self):
        for text in ['nan', 'inf', '-inf', '101', '-1', 'invalid']:
            with self.assertRaises(ValueError):
                Station.value(Mock(get=lambda: text), 0, 100)
        self.assertEqual(Station.value(Mock(get=lambda: '0.01'), 0, 100), .01)

    def test_wire_format_and_sequence_wrap(self):
        station = Station.__new__(Station)
        station.seq = 0xffffffff
        station.sock = Mock()
        station.pending = {}
        station.send('CMD', roll=0., pitch=0., yaw=0., climb=0.)
        raw, target = station.sock.sendto.call_args.args
        self.assertEqual(target, ('192.168.4.1', 4210))
        self.assertEqual(json.loads(raw), dict(type='CMD', seq=0, roll=0, pitch=0, yaw=0, climb=0))

    def test_ack_updates_current_and_entries(self):
        station = Station.__new__(Station)
        station.pending = {4: (0, 'GET')}
        station.ack = Mock()
        station.current = [Mock() for _ in range(6)]
        station.entries = [[Mock(get=lambda: '') for _ in range(3)] for _ in range(6)]
        gains = [[1.5, 0, .6]]*6
        station.receive(dict(type='ACK', seq=4, reason='OK', gains=gains))
        self.assertNotIn(4, station.pending)
        station.entries[0][0].set.assert_called_with('1.5')

    def test_continuous_throttle_wire_format(self):
        station = Station.__new__(Station)
        station.seq = 42
        station.sock = Mock()
        station.pending = {}
        station.send('CMD', roll=0., pitch=0., yaw=0., climb=0., throttle=1400.)
        raw, _ = station.sock.sendto.call_args.args
        self.assertEqual(json.loads(raw)['throttle'], 1400.)

    def test_manual_transition_adopts_actual_collective(self):
        station = Station.__new__(Station)
        station.seq = 1
        station.sock = Mock()
        station.pending = {}
        station.throttle = Mock()
        station.telemetry = {'collective_us': 1432.5}
        station.send('MANUAL')
        station.throttle.set.assert_called_once_with(1432.5)

    def test_arm_requires_token_and_consumes_local_copy(self):
        station = Station.__new__(Station)
        station.seq = 1
        station.sock = Mock()
        station.pending = {}
        station.ack = Mock()
        station.telemetry = {}
        station.send('ARM')
        station.sock.sendto.assert_not_called()
        station.telemetry['arm_token'] = 72
        station.send('ARM')
        raw, _ = station.sock.sendto.call_args.args
        self.assertEqual(json.loads(raw)['arm_token'], 72)
        self.assertNotIn('arm_token', station.telemetry)

    def test_stop_forgets_token_and_clear_uses_latest_token(self):
        station = Station.__new__(Station)
        station.seq = 1
        station.sock = Mock()
        station.pending = {}
        station.ack = Mock()
        station.telemetry = {'arm_token': 9}
        station.send('STOP')
        self.assertNotIn('arm_token', station.telemetry)
        station.receive(dict(type='ACK',seq=2,reason='OK',arm_token=10))
        station.send('CLEAR_FAULT')
        raw, _ = station.sock.sendto.call_args.args
        self.assertEqual(json.loads(raw)['arm_token'],10)
        self.assertNotIn('arm_token', station.telemetry)


if __name__ == '__main__':
    unittest.main()
