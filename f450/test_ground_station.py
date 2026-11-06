import json
import unittest
from unittest.mock import Mock
from f450_ground_station import Station


class GroundStationTests(unittest.TestCase):
    def make_station(self):
        station = Station.__new__(Station)
        station.session_id = 1
        station.owns_session = True
        station.discovery_seq = station.hello_seq = None
        station.seq = 1
        station.sock = Mock()
        station.pending = {}
        station.telemetry = {}
        station.ack = Mock()
        return station
    def test_numeric_rejection(self):
        for text in ['nan', 'inf', '-inf', '101', '-1', 'invalid']:
            with self.assertRaises(ValueError):
                Station.value(Mock(get=lambda: text), 0, 100)
        self.assertEqual(Station.value(Mock(get=lambda: '0.01'), 0, 100), .01)

    def test_wire_format_and_sequence_wrap(self):
        station = self.make_station()
        station.seq = 0xffffffff
        station.sock = Mock()
        station.pending = {}
        station.send('CMD', roll=0., pitch=0., yaw=0., climb=0.)
        raw, target = station.sock.sendto.call_args.args
        self.assertEqual(target, ('192.168.4.1', 4210))
        self.assertEqual(json.loads(raw), dict(type='CMD', seq=0, session=1, roll=0, pitch=0, yaw=0, climb=0))

    def test_ack_updates_current_and_entries(self):
        station = self.make_station()
        station.pending = {4: (0, 'GET')}
        station.ack = Mock()
        station.current = [Mock() for _ in range(6)]
        station.entries = [[Mock(get=lambda: '') for _ in range(3)] for _ in range(6)]
        gains = [[1.5, 0, .6]]*6
        station.receive(dict(type='ACK', seq=4, reason='OK', gains=gains))
        self.assertNotIn(4, station.pending)
        station.entries[0][0].set.assert_called_with('1.5')

    def test_continuous_throttle_wire_format(self):
        station = self.make_station()
        station.seq = 42
        station.sock = Mock()
        station.pending = {}
        station.send('CMD', roll=0., pitch=0., yaw=0., climb=0., throttle=1400.)
        raw, _ = station.sock.sendto.call_args.args
        self.assertEqual(json.loads(raw)['throttle'], 1400.)

    def test_manual_transition_adopts_actual_collective(self):
        station = self.make_station()
        station.seq = 1
        station.sock = Mock()
        station.pending = {}
        station.throttle = Mock()
        station.telemetry = {'collective_us': 1432.5}
        station.send('MANUAL')
        station.throttle.set.assert_called_once_with(1432.5)

    def test_arm_requires_token_and_consumes_local_copy(self):
        station = self.make_station()
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
        station = self.make_station()
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

    def test_acquisition_waits_for_hello_ack(self):
        station = self.make_station()
        station.acquire()
        discover = station.discovery_seq
        self.assertFalse(station.owns_session)
        station.receive(dict(type='ACK',seq=discover,ok=True,reason='OK',session=20,owned=False,owner_active=False,arm_token=50))
        raw, _ = station.sock.sendto.call_args.args
        self.assertEqual(json.loads(raw)['type'],'HELLO')
        self.assertEqual(json.loads(raw)['session'],20)
        self.assertFalse(station.owns_session)
        station.receive(dict(type='ACK',seq=station.hello_seq,ok=True,reason='OK',session=20,owned=True,arm_token=51))
        self.assertTrue(station.owns_session)

    def test_release_success_then_new_session(self):
        station = self.make_station()
        seq = station.send('RELEASE')
        station.receive(dict(type='ACK',seq=seq,ok=True,reason='OK',session=2,owned=False))
        self.assertFalse(station.owns_session)
        self.assertIsNone(station.session_id)
        station.sock.reset_mock()
        station.send('CMD',roll=0,pitch=0,yaw=0,climb=0)
        station.sock.sendto.assert_not_called()
        station.acquire()
        station.receive(dict(type='ACK',seq=station.discovery_seq,ok=True,reason='OK',session=2,owned=False,owner_active=False))
        station.receive(dict(type='ACK',seq=station.hello_seq,ok=True,reason='OK',session=2,owned=True))
        self.assertTrue(station.owns_session)
        self.assertEqual(station.session_id,2)

    def test_release_rejection_preserves_only_confirmed_owner(self):
        station = self.make_station()
        seq = station.send('RELEASE')
        station.receive(dict(type='ACK',seq=seq,ok=False,reason='REQUIRES_DISARMED',session=1,owned=True))
        self.assertTrue(station.owns_session)
        self.assertEqual(station.session_id,1)
        seq = station.send('RELEASE')
        station.receive(dict(type='ACK',seq=seq,ok=False,reason='NO_SESSION',session=2,owned=False))
        self.assertFalse(station.owns_session)

    def test_resume_synchronizes_sequence_and_foreign_owner_blocks(self):
        station = self.make_station()
        station.acquire()
        station.receive(dict(type='ACK',seq=station.discovery_seq,ok=True,reason='OK',session=1,owned=True,owner_active=True,seq_set=True,last_seq=0xffffffff))
        raw, _ = station.sock.sendto.call_args.args
        self.assertEqual(json.loads(raw)['seq'],0)
        self.assertFalse(station.owns_session)
        station.receive(dict(type='ACK',seq=station.hello_seq,ok=False,reason='OLD_SEQUENCE',session=1,owned=True))
        self.assertFalse(station.owns_session)
        station.acquire()
        station.sock.reset_mock()
        station.receive(dict(type='ACK',seq=station.discovery_seq,ok=True,reason='OK',session=1,owned=False,owner_active=True))
        station.sock.sendto.assert_not_called()
        self.assertFalse(station.owns_session)

    def test_uncorrelated_old_ack_cannot_change_new_ownership(self):
        station = self.make_station()
        station.receive(dict(type='ACK',seq=999,ok=True,reason='OK',session=0,owned=False))
        self.assertTrue(station.owns_session)

    def test_reconnect_is_blocked_while_armed(self):
        station = self.make_station()
        station.telemetry['armed'] = True
        station.acquire()
        station.sock.sendto.assert_not_called()
        self.assertTrue(station.owns_session)


if __name__ == '__main__':
    unittest.main()
