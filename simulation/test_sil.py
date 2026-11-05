import copy
import math
import unittest
import numpy as np
from .native import Library
from .engine import load_config, Simulation
from .models import Quadrotor, Motors, ImuModel, Link, quaternion, rotation, euler
from .experiments import network_experiment


class NativeAndModelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.lib=Library(); cls.config=load_config(); cls.fixture=cls.config["analog_fixture"]

    def test_hardware_output_gates_remain_false(self):
        self.assertEqual([self.lib.config[k] for k in ("outputs","power","propulsion","controller")],[0]*4)

    def test_native_identity_calibration_and_zero_bias(self):
        with self.lib.context(self.fixture,False) as c:
            for i in range(1100):
                d=c.drone([0,0,1,.5,-.25,.2],.004,i*4)
            self.assertEqual(d[6],1)
            self.assertLess(max(abs(v) for v in d[41:44]),.02)
            self.assertEqual(d[3],3)
            self.assertEqual(d[16:20],[0]*4)

    def test_uncalibrated_motion_cannot_arm(self):
        with self.lib.context(self.fixture,True) as c:
            for i in range(1500): d=c.drone([0,0,1,20,0,0],.004,i*4)
            self.assertEqual(d[6],0)
            self.assertNotEqual(d[3],5)
            self.assertEqual(d[16:20],[0]*4)

    def test_filter_level_bias_and_yaw_observability(self):
        with self.lib.filter() as f:
            for _ in range(5000): ok,value=f.update([0,0,1,.3,-.2,.5],.004)
            self.assertTrue(ok)
            self.assertLess(abs(value[0]),.3)
            self.assertLess(abs(value[1]),.2)
            self.assertGreater(value[2],9)
            self.assertLess(value[2],11)

    def test_filter_invalid_data_and_recovery(self):
        with self.lib.filter() as f:
            self.assertTrue(f.update([0,0,1,0,0,0],.004)[0])
            for sample in ([np.nan,0,1,0,0,0],[0,0,8,0,0,0],[0,0,1,500,0,0]):
                self.assertFalse(f.update(sample,.004)[0])
                self.assertTrue(f.update([0,0,1,0,0,0],.004)[0])
            self.assertFalse(f.update([0,0,1,0,0,0],.012)[0])

    def test_filter_acceleration_ambiguity_is_detected(self):
        with self.lib.filter() as f:
            for _ in range(2000): _,value=f.update([-.3,0,1,0,0,0],.004)
            self.assertGreater(value[1],15)
            self.assertLess(value[1],18)
        # Known limitation: norm gate cannot distinguish this acceleration from tilt.

    def test_filter_rejects_envelope_crossing_without_corrupting_state(self):
        p=math.radians(59.9)
        with self.lib.filter() as f:
            ok,angles=f.update([-math.sin(p),0,math.cos(p),0,494,0],.004)
            self.assertFalse(ok)
            self.assertLessEqual(abs(angles[1]),60)
            self.assertTrue(f.update([0,0,1,0,0,0],.004)[0])
        with self.lib.filter() as f:
            self.assertFalse(f.update([-1,0,.01,0,0,0],.004)[0])
            self.assertTrue(f.update([0,0,1,0,0,0],.004)[0])

    def test_flex_hysteresis_and_fault(self):
        with self.lib.context(self.fixture,False) as c:
            for _ in range(100): out=c.analog([3000,1200,1000])
            self.assertEqual(out[5],1)
            for i in range(500):
                out=c.analog([2400+20*math.sin(i),1200,1000])
                self.assertEqual(out[5],1)
            for _ in range(200): out=c.analog([1000,1200,1000])
            self.assertEqual(out[5],0)
            self.assertEqual(c.analog([0,1200,1000])[0],0)

    def test_pot_monotonic_full_range(self):
        with self.lib.context(self.fixture,False) as c:
            previous=0
            for value in np.linspace(1000,3000,1001):
                out=c.analog([1000,1200,value])
                self.assertEqual(out[2],1)
                self.assertGreaterEqual(out[7],previous-1e-6)
                self.assertTrue(0<=out[7]<=1)
                previous=out[7]
            self.assertEqual(previous,1)
            self.assertEqual(c.analog([1000,1200,1000])[8],0)

    def test_pot_rejects_rails_and_preserves_stuck_limitation(self):
        with self.lib.context(self.fixture,False) as c:
            for value in [0,4095,850,3150]:
                out=c.analog([1000,1200,value]); self.assertEqual(out[2],0); self.assertEqual(out[8],0)
            for _ in range(500): out=c.analog([1000,1200,2000])
            self.assertEqual(out[2],1)
            self.assertAlmostEqual(out[8],(.5-.02)/.98,places=4)
        # Plausible stuck ADC is intrinsically indistinguishable from a held knob.

    def test_pid_antiwindup_and_recovery(self):
        with self.lib.context(self.fixture,False) as c:
            for _ in range(3000): value,terms=c.pid(1000,0)
            self.assertLessEqual(value,.200001)
            self.assertEqual(terms[0],0)
            value,_=c.pid(0,0)
            self.assertEqual(value,0)
            for _ in range(1000): value,terms=c.pid(10,0)
            self.assertAlmostEqual(terms[0],.008,places=5)
            self.assertAlmostEqual(value,.018,places=5)

    def test_mixer_configured_geometry_and_endpoints(self):
        cases=[([.5,.1,0,0],[.4,.6,.6,.4]),([.5,0,.1,0],[.6,.6,.4,.4]),
               ([.5,0,0,.1],[.6,.4,.6,.4])]
        for values,expected in cases:
            np.testing.assert_allclose(self.lib.mix(values)[2:6],expected,atol=1e-6)
        for throttle,pulse in [(0,1000),(1,2000)]:
            v=self.lib.mix([throttle,0,0,0])
            for duty in v[6:10]: self.assertAlmostEqual(duty*1e6/(50*65535),pulse,delta=.31)
        sat=self.lib.mix([.9,.8,.8,.8])
        self.assertEqual(sat[1],1);self.assertTrue(all(0<=x<=1 for x in sat[2:6]))
        self.assertEqual(self.lib.mix([np.nan,0,0,0])[6:10],[0]*4)

    def test_receiver_rejects_duplicate_corrupt_invalid_and_old_sequence(self):
        with self.lib.context(self.fixture,False) as c:
            p=self.lib.encode([0,0,0,0],1)
            self.assertTrue(c.receive(p,0))
            self.assertFalse(c.receive(p,4))
            self.assertTrue(c.receive(self.lib.encode([0,0,0,0],3),8))
            self.assertFalse(c.receive(self.lib.encode([0,0,0,0],2),12))
            bad=bytearray(self.lib.encode([0,0,0,0],4));bad[12]^=1
            self.assertFalse(c.receive(bytes(bad),16))
            self.assertFalse(c.receive(self.lib.encode([1001,0,0,0],5),20))
            self.assertFalse(c.receive(p[:-1],24))

    def test_sender_age_is_not_in_current_protocol(self):
        with self.lib.context(self.fixture,False) as c:
            self.assertTrue(c.receive(self.lib.encode([0,0,0,0],1),0))
            # A previously unseen sequence generated long ago has no timestamp on wire.
            self.assertTrue(c.receive(self.lib.encode([0,0,0,0],2),5000))
            d=c.drone([0,0,1,0,0,0],.004,5000)
            self.assertEqual(d[7],0)

    def test_network_impairments_reproducible(self):
        a=network_experiment(self.lib,self.config);b=network_experiment(self.lib,self.config)
        self.assertEqual(a,b)
        self.assertTrue(25<a["dropped"]<70)
        self.assertTrue(30<a["rejected"]<150)
        self.assertTrue(150<a["accepted"]<300)
        self.assertAlmostEqual(a["unique_delivery_rate"],.85)
        self.assertGreater(a["arrival_copies_per_sent"],1)
        self.assertLessEqual(a["latency_max_ms"],72.0001) # 30+40 ms jitter bound + 2 ms duplicate delay.

    def test_free_fall_and_motor_response(self):
        p=copy.deepcopy(self.config["vehicle"]);p["linear_drag_N_s_m"]=0
        q=Quadrotor(p)
        for _ in range(100): q.step([0]*4,.004)
        self.assertAlmostEqual(q.velocity[2],p["gravity_m_s2"]*.4,places=6)
        self.assertAlmostEqual(q.position[2],-2+.5*p["gravity_m_s2"]*.4**2,delta=.01)
        motor=Motors(p); thrust=motor.step([2000]*4,.004)
        self.assertGreater(thrust[0],0);self.assertLess(thrust[0],p["kt_N_s2"]*p["omega_max_rad_s"]**2*.02)
        for _ in range(250): thrust=motor.step([2000]*4,.004)
        self.assertAlmostEqual(thrust[0],p["kt_N_s2"]*p["omega_max_rad_s"]**2,places=5)

    def test_physics_torque_signs_independent_of_mixer(self):
        for axis,input_values in enumerate(([.6,.1,0,0],[.6,0,.1,0],[.6,0,0,.1])):
            commands=self.lib.mix(input_values)[2:6]
            q=Quadrotor(self.config["vehicle"])
            for _ in range(20):q.step(1000+1000*np.array(commands),.004)
            self.assertGreater(q.omega[axis],0)

    def test_rotation_and_specific_force_conventions(self):
        for angles in ([20,0,0],[0,-20,0],[15,25,-30]):
            q=quaternion(angles)
            np.testing.assert_allclose(euler(q),angles,atol=1e-10)
            np.testing.assert_allclose(rotation(q).T@rotation(q),np.eye(3),atol=1e-12)
        p=copy.deepcopy(self.config["sensor"]);p.update(accel_noise_g=0,gyro_noise_dps=0)
        model=ImuModel(p,1)
        # A true free-fall accelerometer reports zero specific force.
        np.testing.assert_allclose(model.sample(quaternion([0,0,0]),[0,0,0],[0,0,9.80665],.004,0)[:3],0)

    def test_end_to_end_startup_arm_and_hover_attitude(self):
        s=Simulation(library=self.lib)
        try:
            rows=s.run(16)
            self.assertEqual(rows[-1]["state"],5)
            self.assertTrue(all(max(r["pwm"])==0 for r in rows if r["t"]<4))
            self.assertLess(max(abs(x) for x in rows[-1]["truth"][:2]),1)
            self.assertGreater(sum(rows[-1]["thrust"]),14)
            self.assertLess(sum(rows[-1]["thrust"]),15.5)
        finally:s.close()

    def test_faults_latch_and_outputs_inhibit(self):
        for name,mask in [("wireless_loss",8),("imu_failure",1),("missed_deadline",2)]:
            s=Simulation(scenario=name,library=self.lib)
            try:
                rows=s.run(15)
                self.assertIn(rows[-1]["state"],(6,7))
                self.assertTrue(rows[-1]["faults"]&mask)
                self.assertEqual(rows[-1]["pwm"],[0]*4)
                if name=="wireless_loss":self.assertTrue(12.17<=s.fault_time<=12.23)
            finally:s.close()

    def test_trace_and_network_buffers_are_bounded(self):
        c=copy.deepcopy(self.config);c["trace_limit"]=20
        s=Simulation(config=c,library=self.lib)
        try:
            s.run(3)
            self.assertEqual(len(s.trace),20)
        finally:s.close()
        p=copy.deepcopy(c["link"]);p.update(queue_limit=4,delay_s=10,jitter_s=0)
        link=Link(p,1)
        for i in range(100):link.send(b"x",i*.01)
        self.assertEqual(len(link.queue),4)


if __name__ == "__main__": unittest.main()
