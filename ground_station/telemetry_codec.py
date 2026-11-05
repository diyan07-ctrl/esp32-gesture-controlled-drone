"""Strict decoder for the 142-byte, CRC-checked BNO055 drone bench datagram."""
import binascii
import math
import struct

SIZE = 142
VERSION = 2
STATES = ("BOOT", "IMU_INITIALIZATION", "IMU_CALIBRATION",
          "WIRELESS_INITIALIZATION", "WAITING_FOR_GLOVE", "READY_SENSOR_LINK_ONLY",
          "COMMUNICATION_LOST", "SENSOR_FAULT", "SYSTEM_FAULT")
FLAG_NAMES = ("imu_initialized", "calibrated", "estimate_valid", "espnow_initialized",
              "peer_configured", "wifi_connected", "link_fresh", "outputs_enabled",
              "bno055", "sensor_valid")


class TelemetryError(ValueError):
    pass


def decode(payload):
    if len(payload) != SIZE:
        raise TelemetryError(f"length {len(payload)}; expected {SIZE}")
    if payload[:2] != b"FT" or payload[2] != VERSION or payload[3] != 2:
        raise TelemetryError("magic or version mismatch")
    if struct.unpack_from("<H", payload, 4)[0] != SIZE:
        raise TelemetryError("declared length mismatch")
    actual = struct.unpack_from("<H", payload, 140)[0]
    expected = binascii.crc_hqx(payload[:140], 0xffff)
    if actual != expected:
        raise TelemetryError("CRC mismatch")
    flags = struct.unpack_from("<H", payload, 6)[0]
    state, channel, gesture, command_flags = struct.unpack_from("<4B", payload, 16)
    if flags & ~895 or state >= len(STATES) or channel > 14 or gesture > 11 or command_flags & ~7:
        raise TelemetryError("invalid status field")
    sequence, uptime = struct.unpack_from("<2I", payload, 8)
    counters = struct.unpack_from("<12I", payload, 20)
    throttle, roll_command, pitch_command, yaw_command, thumb, index = struct.unpack_from("<HhhhHH", payload, 68)
    if throttle > 1000 or max(abs(x) for x in (roll_command, pitch_command, yaw_command)) > 1000 or max(thumb,index) > 4095:
        raise TelemetryError("invalid command range")
    accel = struct.unpack_from("<3f", payload, 80)
    gyro = struct.unpack_from("<3f", payload, 92)
    roll, pitch, yaw_rate = struct.unpack_from("<3f", payload, 104)
    quat = struct.unpack_from("<4f",payload,120)
    if not all(map(math.isfinite, accel + gyro + (roll,pitch,yaw_rate) + quat)):
        raise TelemetryError("nonfinite sensor field")
    status = {name:bool(flags & (1 << i)) for i,name in enumerate(FLAG_NAMES)}
    if status["outputs_enabled"]:
        raise TelemetryError("bench protocol unexpectedly reports physical outputs enabled")
    keys = ("dt_us", "execution_us", "maximum_us", "missed", "imu_errors", "accepted",
            "rejected", "lost", "mailbox_dropped", "send_errors", "command_sequence", "command_age_ms")
    result = {"sequence":sequence, "uptime_ms":uptime, "state":STATES[state], "channel":channel,
              "gesture":gesture if status["link_fresh"] else None,
              "command_flags":command_flags if status["link_fresh"] else None,
              "throttle":throttle if status["link_fresh"] else None,
              "roll_command":roll_command if status["link_fresh"] else None,
              "pitch_command":pitch_command if status["link_fresh"] else None,
              "thumb_adc":thumb if status["link_fresh"] else None,
              "index_adc":index if status["link_fresh"] else None,
              "accel_g":accel if status["sensor_valid"] else None,
              "gyro_dps":gyro if status["sensor_valid"] else None,
              "roll_deg":roll if status["estimate_valid"] else None,
              "pitch_deg":pitch if status["estimate_valid"] else None,
              "yaw_rate_dps":yaw_rate if status["estimate_valid"] else None,
              "quaternion_sensor_frame":quat if status["estimate_valid"] else None,
              "calibration_status":payload[136] if status["sensor_valid"] else None,
              "system_status":payload[137] if status["sensor_valid"] else None,
              "system_error":payload[138] if status["sensor_valid"] else None,
              "wifi_reconnects":struct.unpack_from("<I",payload,116)[0], **status}
    result.update(zip(keys,counters))
    if result["command_age_ms"] == 0xffffffff:
        result["command_age_ms"] = None
    return result
