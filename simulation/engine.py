"""Deterministic scheduler for actual native pilot/controller and assumed physics."""
from collections import deque
import copy
import json
import math
from pathlib import Path
import numpy as np
from .native import Library
from .models import Quadrotor, ImuModel, Link, quaternion, body_rates, euler

CONFIG = Path(__file__).parent / "config/default.json"
SCENARIOS = ["startup", "arming", "hover", "forward_backward", "left_right", "throttle",
             "fingers", "wireless_loss", "imu_failure", "disturbance", "extreme", "long_duration",
             "roll_step", "pitch_step", "combined_step", "saturation", "timing_jitter", "missed_deadline"]


def load_config(path=None):
    config = json.loads(Path(path or CONFIG).read_text(encoding="utf-8"))
    p = config["vehicle"]
    for key in ("mass_kg","diagonal_m","kt_N_s2","kq_Nm_s2","omega_max_rad_s","motor_tau_s","esc_hz"):
        if not math.isfinite(p[key]) or p[key] <= 0:
            raise ValueError(f"invalid vehicle parameter {key}")
    if len(p["inertia_kg_m2"]) != 3 or min(p["inertia_kg_m2"]) <= 0:
        raise ValueError("inertia must be positive")
    if not .002 <= config["dt"] <= .006:
        raise ValueError("dt outside firmware's tested timing contract")
    return config


class Simulation:
    def __init__(self, config=None, scenario="hover", library=None):
        self.config = copy.deepcopy(config or load_config())
        self.library = library or Library()
        self.scenario = scenario
        self.native = self.library.context(self.config["analog_fixture"], self.config["virtual_actuation_permit"])
        self.plant = Quadrotor(self.config["vehicle"])
        self.glove_imu = ImuModel(self.config["sensor"], self.config["seed"])
        self.drone_imu = ImuModel(self.config["sensor"], self.config["seed"]+1)
        self.link = Link(self.config["link"], self.config["seed"]+2)
        self.rng = np.random.default_rng(self.config["seed"]+3)
        self.time = 0.0
        self.hand = np.zeros(3)
        self.last_ack = -100.0
        self.ack_state = 0
        self.last_ack_update = -1.0
        self.drone = [0.0]*48
        self.glove = [0.0]*19
        self.trace = deque(maxlen=self.config["trace_limit"])
        self.steps = 0
        self.next_record = 0.0
        self.direct_sequence = 100000
        self.fault_time = None
        self.max_native_us = 0.0
        self.saturation_time = 0.0
        self.first_fault_age_ms = None
        self.callback_overwrites = 0
        self.duration = 120 if scenario == "long_duration" else 20

    def close(self): self.native.close()

    def controls(self):
        t = self.time
        h = self.plant.hover_command
        # Native firmware deadband is applied after endpoint normalization.
        hover_pot = self.library.config["pot_deadband"] + (1-self.library.config["pot_deadband"])*h
        controls = [0.,0., 0. if t < 6.8 else hover_pot,
                    1. if 4.4 <= t < 6.8 else 0., 1. if 4.4 <= t < 6.8 else 0.]
        if self.scenario == "startup": controls[3:] = [0,0]
        if self.scenario == "forward_backward":
            controls[1] = -20 if 10 <= t < 11 else 20 if 12 <= t < 13 else 0
        if self.scenario == "left_right":
            controls[0] = -20 if 10 <= t < 11 else 20 if 12 <= t < 13 else 0
        if self.scenario == "throttle" and t >= 10:
            controls[2] = hover_pot + (.06 if t < 12 else -.06 if t < 14 else 0)
        if self.scenario == "fingers" and 10 <= t < 11: controls[3] = 1
        if self.scenario == "fingers" and 12 <= t < 13: controls[4] = 1
        if self.scenario == "extreme" and 10 <= t < 11:
            controls[0:3] = [45,-45,1]
        return controls

    def step(self, manual=None, emergency=False):
        dt = self.config["dt"]
        if self.scenario == "timing_jitter": dt *= 1+self.rng.uniform(-.2,.2)
        if self.scenario == "missed_deadline" and 12 <= self.time < 12.004: dt = .012
        t = self.time
        controls = list(manual) if manual is not None and t >= 6.8 else self.controls()
        if emergency: controls = [0,-25,0,1,1]
        old = self.hand.copy()
        self.hand += (np.asarray([controls[0],controls[1],0])-self.hand)*(1-math.exp(-dt/.07))
        rates = body_rates(self.hand, (self.hand-old)/dt)
        hand_sensor = self.glove_imu.sample(quaternion(self.hand), np.radians(rates), [0,0,0], dt,t)
        fixture = t < 9.5
        sensor_mode = "gravity_only" if self.scenario in ("roll_step","pitch_step","combined_step","saturation") else "specific_force"
        drone_sensor = self.drone_imu.sample(self.plant.q,self.plant.omega,self.plant.accel,dt,t,sensor_mode)
        fault = 0
        if self.scenario == "imu_failure" and 12 <= t < 12.1: fault = 1
        f = self.config["analog_fixture"]
        adc = np.array([f["thumb_open"]+(f["thumb_bent"]-f["thumb_open"])*controls[3],
                        f["index_open"]+(f["index_bent"]-f["index_open"])*controls[4],
                        f["pot_low"]+(f["pot_high"]-f["pot_low"])*controls[2]])
        adc = np.clip(np.rint(adc+self.rng.normal(0,self.config["sensor"]["adc_noise_counts"],3)),0,4095)
        fresh = t-self.last_ack <= .3
        self.glove, packet = self.native.glove(hand_sensor,adc,dt,round(t*1000),fresh,self.ack_state)
        direct = self.scenario in ("roll_step","pitch_step","combined_step","saturation") and t >= 10
        if packet:
            if direct:
                self.direct_sequence += 1
                roll = 250 if self.scenario in ("roll_step","combined_step") else 0
                pitch = 250 if self.scenario in ("pitch_step","combined_step") else 0
                throttle = self.plant.hover_command*1000
                if self.scenario == "saturation": roll,pitch,throttle=1000,1000,980
                packet = self.library.encode([throttle,roll,pitch,0],self.direct_sequence,3)
            self.link.send(packet,t,self.scenario == "wireless_loss" and 12 <= t < 13)
        arrivals = self.link.receive(t)
        if arrivals:
            self.callback_overwrites += max(0,len(arrivals)-1)
            emergency_packets = [entry for entry in arrivals if len(entry[1])>3 and entry[1][3]&4]
            arrivals = [emergency_packets[-1] if emergency_packets else arrivals[-1]]
        for arrival, packet in arrivals:
            accepted = self.native.receive(packet,round(arrival*1000))
            if accepted and t-self.last_ack_update >= .1:
                # Idealized return status channel; forward data path has the configured RF model.
                self.last_ack = t; self.last_ack_update=t; self.ack_state=int(self.drone[3])
        self.drone = self.native.drone(drone_sensor,dt,round(t*1000),fault)
        self.max_native_us = max(self.max_native_us,self.drone[10])
        self.saturation_time += dt if self.drone[11] else 0
        if self.drone[3] in (6,7) and self.fault_time is None:
            self.fault_time=t;self.first_fault_age_ms=self.drone[7]
        torque = [0,.03,0] if self.scenario == "disturbance" and 12 <= t < 12.16 else [0,0,0]
        force = [1,0,0] if self.scenario == "disturbance" and 12 <= t < 12.3 else [0,0,0]
        # A missed control interval still integrates plant at <=4ms substeps.
        count = max(1,math.ceil(dt/.004))
        for _ in range(count):
            self.plant.step(self.drone[16:20],dt/count,torque,force,fixture)
        self.time += dt; self.steps += 1
        snapshot = self.snapshot(adc,hand_sensor,drone_sensor,fixture)
        if self.time >= self.next_record:
            self.trace.append(snapshot)
            self.next_record += 1/self.config["trace_hz"]
        return snapshot

    def snapshot(self, adc=None, hand_sensor=None, drone_sensor=None, fixture=False):
        p = self.plant
        return {"t":self.time,"truth":euler(p.q).tolist(),"q":p.q.tolist(),
            "position":p.position.tolist(),"velocity":p.velocity.tolist(),"omega":np.degrees(p.omega).tolist(),
            "estimate":self.drone[:3],"state":int(self.drone[3]),"faults":int(self.drone[4]),
            "imu_health":bool(self.drone[5]),"calibrated":bool(self.drone[6]),
            "cal_samples":int(self.drone[47]),"loop_dt_s":self.drone[46],
            "motor":self.drone[12:16],"pwm":self.drone[16:20],
            "rpm":(p.motors.omega*60/(2*np.pi)).tolist(),"thrust":p.thrust.tolist(),
            "setpoint":self.drone[20:23],"rate_target":self.drone[23:26],
            "pid_p":self.drone[26:29],"pid_i":self.drone[29:32],"pid_d":self.drone[32:35],
            "pid":self.drone[35:38],"imu":list(drone_sensor) if drone_sensor is not None else self.drone[38:44],
            "pid_error":[self.drone[23+i]-self.drone[41+i] for i in range(3)],
            "glove":self.glove,"adc":list(adc) if adc is not None else [0,0,0],
            "hand_imu":list(hand_sensor) if hand_sensor is not None else [0]*6,
            "age_ms":self.drone[7],"lost":self.drone[8],"rejected":self.drone[9],
            "latency_ms":1000*self.link.latencies[-1] if self.link.latencies else 0,
            "sent":self.link.sent,"dropped":self.link.dropped,"delivered":self.link.delivered,
            "saturated":bool(self.drone[11]),"host_native_us":self.drone[10],"fixture":fixture}

    def run(self, duration=None):
        while self.time < (duration or self.duration): self.step()
        return list(self.trace)
