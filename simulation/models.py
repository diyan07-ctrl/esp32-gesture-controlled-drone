"""Assumed rigid-body plant, sensor and network HALs. SI units; body FRD, world NED."""
import heapq
import math
import numpy as np


def quaternion(euler_deg):
    r, p, y = np.radians(euler_deg) / 2
    cr, cp, cy, sr, sp, sy = math.cos(r), math.cos(p), math.cos(y), math.sin(r), math.sin(p), math.sin(y)
    return np.array([cr*cp*cy+sr*sp*sy, sr*cp*cy-cr*sp*sy,
                     cr*sp*cy+sr*cp*sy, cr*cp*sy-sr*sp*cy])


def rotation(q):
    w, x, y, z = q
    return np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                     [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                     [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]])


def euler(q):
    r = rotation(q)
    return np.degrees([math.atan2(r[2,1], r[2,2]),
                       math.asin(np.clip(-r[2,0], -1, 1)), math.atan2(r[1,0], r[0,0])])


def multiply(a, b):
    w, x, y, z = a; v, i, j, k = b
    return np.array([w*v-x*i-y*j-z*k, w*i+x*v+y*k-z*j,
                     w*j-x*k+y*v+z*i, w*k+x*j-y*i+z*v])


def body_rates(euler_deg, derivative_deg_s):
    r, p, _ = np.radians(euler_deg)
    rd, pd, yd = derivative_deg_s
    return np.array([rd-yd*math.sin(p), pd*math.cos(r)+yd*math.sin(r)*math.cos(p),
                     -pd*math.sin(r)+yd*math.cos(r)*math.cos(p)])


class Motors:
    def __init__(self, parameters):
        self.p = parameters
        self.omega = np.zeros(4)
        self.held = np.zeros(4)
        self.next_frame = 0.0
        self.time = 0.0

    def step(self, pwm_us, dt):
        if self.time + 1e-9 >= self.next_frame:
            self.held = np.clip((np.asarray(pwm_us)-1000)/1000, 0, 1)
            self.next_frame += 1/self.p["esc_hz"]
        target = self.p["omega_max_rad_s"] * self.held
        self.omega += (target-self.omega) * (1-math.exp(-dt/self.p["motor_tau_s"]))
        self.time += dt
        return self.p["kt_N_s2"] * self.omega**2


class Quadrotor:
    def __init__(self, parameters):
        self.p = parameters
        self.position = np.array([0.0, 0.0, -2.0])
        self.velocity = np.zeros(3)
        self.q = quaternion([0, 0, 0])
        self.omega = np.zeros(3)
        self.alpha = np.zeros(3)
        self.accel = np.zeros(3)
        self.motors = Motors(parameters)
        self.thrust = np.zeros(4)
        a = parameters["diagonal_m"]/(2*math.sqrt(2))
        self.mounts = np.array([[a,a,0], [a,-a,0], [-a,-a,0], [-a,a,0]])
        self.inertia = np.asarray(parameters["inertia_kg_m2"])

    @property
    def hover_command(self):
        p = self.p
        return math.sqrt(p["mass_kg"]*p["gravity_m_s2"]/(4*p["kt_N_s2"]*p["omega_max_rad_s"]**2))

    def step(self, pwm_us, dt, torque=None, force=None, fixture=False):
        if not 0 < dt <= .02:
            raise ValueError("plant step must be in (0,.02] seconds")
        self.thrust = self.motors.step(pwm_us, dt)
        if fixture:
            self.accel[:] = 0; self.omega[:] = 0; self.alpha[:] = 0
            return
        forces = np.zeros((4,3)); forces[:,2] = -self.thrust
        moment = np.cross(self.mounts, forces).sum(axis=0)
        moment[2] = self.p["kq_Nm_s2"] * np.dot([1,-1,1,-1], self.motors.omega**2)
        moment += np.asarray(torque if torque is not None else [0,0,0])
        moment -= self.p["angular_drag_Nm_s"]*self.omega
        self.alpha = (moment-np.cross(self.omega, self.inertia*self.omega))/self.inertia
        old_omega = self.omega.copy()
        self.omega += dt*self.alpha
        mid = (old_omega+self.omega)*.5
        angle = np.linalg.norm(mid)*dt
        if angle > 1e-14:
            dq = np.r_[math.cos(angle/2), mid/np.linalg.norm(mid)*math.sin(angle/2)]
            self.q = multiply(self.q, dq)
            self.q /= np.linalg.norm(self.q)
        total_force = rotation(self.q) @ forces.sum(axis=0)
        total_force -= self.p["linear_drag_N_s_m"]*self.velocity
        total_force += np.asarray(force if force is not None else [0,0,0])
        self.accel = total_force/self.p["mass_kg"] + [0,0,self.p["gravity_m_s2"]]
        self.velocity += self.accel*dt
        self.position += self.velocity*dt
        # Simple non-bouncing floor, not a landing/contact dynamics model.
        if self.position[2] > 0:
            self.position[2] = 0
            self.velocity[2] = min(0, self.velocity[2])
            self.accel[2] = min(0, self.accel[2])


class ImuModel:
    def __init__(self, parameters, seed):
        self.p = parameters
        self.rng = np.random.default_rng(seed)
        self.filtered = None

    def sample(self, q, omega, accel, dt, time, mode="specific_force"):
        gravity = np.array([0,0,9.80665])
        # Firmware's acceleration convention is negative specific force, in g.
        a = rotation(q).T @ (gravity - (np.asarray(accel) if mode == "specific_force" else 0))/9.80665
        rates = np.degrees(omega) + np.asarray(self.p["gyro_bias_dps"])
        rates += max(0, time-4.2)*np.asarray(self.p["gyro_drift_dps_per_s"])
        a += self.rng.normal(0, self.p["accel_noise_g"], 3)
        rates += self.rng.normal(0, self.p["gyro_noise_dps"], 3)
        value = np.r_[a, rates]
        if self.filtered is None:
            self.filtered = value
        else:
            cut = np.r_[np.full(3,self.p["accel_cutoff_hz"]), np.full(3,self.p["gyro_cutoff_hz"])]
            self.filtered += (value-self.filtered)*(1-np.exp(-2*np.pi*cut*dt))
        # Quantization is performed by the native MPU6050 register HAL.
        return np.clip(self.filtered, [-8]*3+[-500]*3, [8]*3+[500]*3).copy()


class Link:
    def __init__(self, parameters, seed):
        self.p = parameters
        self.rng = np.random.default_rng(seed)
        self.queue = []
        self.sent = self.dropped = self.delivered = self.duplicates = self.corrupted = 0
        self.consecutive = self.max_consecutive = 0
        self.order = 0
        self.latencies = []

    def send(self, packet, time, disconnected=False):
        self.sent += 1
        if disconnected or self.rng.random() < self.p["loss"]:
            self.dropped += 1; self.consecutive += 1
            self.max_consecutive = max(self.max_consecutive, self.consecutive)
            return
        self.consecutive = 0
        if self.rng.random() < self.p["corrupt"]:
            b = bytearray(packet); b[14] ^= 0x80; packet = bytes(b)
            self.corrupted += 1
        copies = 2 if self.rng.random() < self.p["duplicate"] else 1
        self.duplicates += copies-1
        for copy in range(copies):
            delay = max(0, self.p["delay_s"]+self.rng.uniform(-self.p["jitter_s"],self.p["jitter_s"]))
            if len(self.queue) >= self.p["queue_limit"]:
                self.dropped += 1
                continue
            self.order += 1
            heapq.heappush(self.queue, (time+delay+copy*.002,self.order,time,packet))

    def receive(self, time):
        packets = []
        while self.queue and self.queue[0][0] <= time+1e-10:
            arrival, _, sent, packet = heapq.heappop(self.queue)
            self.delivered += 1
            self.latencies.append(arrival-sent)
            if len(self.latencies) > 4096:
                del self.latencies[:2048]
            packets.append((arrival, packet))
        return packets
