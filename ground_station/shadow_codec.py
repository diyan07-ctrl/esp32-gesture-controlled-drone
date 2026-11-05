"""Read-only decoder for the motor-inhibited PID shadow datagram."""
import binascii
import math
import struct

SIZE = 194
FIELDS = ("setpoint", "measured", "error", "p", "i", "d", "output",
          "rate_target", "measured_rate")
AXES = ("roll", "pitch", "yaw")


def decode(packet):
    if len(packet) != SIZE or packet[:3] != b"PS\x01":
        raise ValueError("shadow packet size/magic/version")
    mode = packet[3]
    if mode > 2 or struct.unpack_from("<H", packet, 4)[0] != SIZE or packet[6:8] != b"\0\0":
        raise ValueError("invalid shadow header")
    if struct.unpack_from("<H", packet, 192)[0] != binascii.crc_hqx(packet[:192], 0xffff):
        raise ValueError("shadow CRC")
    if packet[189] > 1 or packet[190:192] != b"\0\0":
        raise ValueError("invalid sensor/output state")
    axes = {}
    values = []
    for index, name in enumerate(AXES):
        data = struct.unpack_from("<9f", packet, 24 + 36 * index)
        axes[name] = dict(zip(FIELDS, data))
        values.extend(data)
    motors = struct.unpack_from("<4f", packet, 132)
    accel = struct.unpack_from("<3f", packet, 148)
    gyro = struct.unpack_from("<3f", packet, 160)
    quaternion = struct.unpack_from("<4f", packet, 172)
    values.extend(motors + accel + gyro + quaternion)
    if not all(math.isfinite(v) for v in values) or any(v < 0 or v > 1 for v in motors):
        raise ValueError("invalid numeric payload")
    if mode != 1 and any(v != 0 for v in motors):
        raise ValueError("motor calculations outside shadow mode")
    sequence, uptime, dt, errors = struct.unpack_from("<4I", packet, 8)
    return {"mode": ("SENSOR", "PID_SHADOW", "MOTOR_LOCKED")[mode],
            "sequence": sequence, "uptime_ms": uptime, "dt_us": dt,
            "imu_errors": errors, "axes": axes, "calculated_motor": motors,
            "accel_g": accel, "gyro_dps": gyro, "quaternion_sensor_frame": quaternion,
            "calibration": packet[188], "sensor_valid": bool(packet[189]),
            "esc_output_enabled": False, "esc_pwm_us": None,
            "actual_motor_operation": None}
