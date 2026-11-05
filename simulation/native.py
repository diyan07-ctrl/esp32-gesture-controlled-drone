"""ctypes adapter. One worker owns each library; no real I/O is exposed."""
import ctypes as ct
import json
from pathlib import Path
from .build_native import LIB, BUILD, sources_hash

D = ct.POINTER(ct.c_double)
B = ct.POINTER(ct.c_uint8)
CONFIG_NAMES = "dt tau flex_tau bend_on bend_off pot_span pot_margin pot_deadband pot_tau pot_rise timeout_ms pwm_hz pwm_min pwm_max max_tilt max_rate max_yaw angle_kp rate_kp rate_ki rate_kd rate_limit outputs power propulsion controller".split()


def doubles(v):
    return (ct.c_double * len(v))(*v)


class Library:
    def __init__(self):
        metadata = BUILD / "build.json"
        if not LIB.exists() or not metadata.exists():
            raise RuntimeError("Build the native bridge first: python -m simulation.build_native")
        if json.loads(metadata.read_text())["sources_sha256"] != sources_hash():
            raise RuntimeError("Native bridge is stale; rerun python -m simulation.build_native")
        self.lib = ct.CDLL(str(LIB))
        signatures = {
            "sil_create": ([D, ct.c_int], ct.c_void_p),
            "sil_destroy": ([ct.c_void_p], None),
            "sil_glove": ([ct.c_void_p, D, D, ct.c_double, ct.c_uint32, ct.c_int, ct.c_int, ct.c_int, B, D], ct.c_int),
            "sil_receive": ([ct.c_void_p, B, ct.c_uint, ct.c_uint32], ct.c_int),
            "sil_drone": ([ct.c_void_p, D, ct.c_double, ct.c_uint32, ct.c_int, D], None),
            "sil_encode": ([D, ct.c_uint32, ct.c_uint32, ct.c_uint, B], None),
            "sil_filter_create": ([ct.c_double], ct.c_void_p),
            "sil_filter_destroy": ([ct.c_void_p], None),
            "sil_filter": ([ct.c_void_p, D, ct.c_double, D], ct.c_int),
            "sil_mix": ([D, D], None),
            "sil_analog": ([ct.c_void_p, D, ct.c_double, D], None),
            "sil_pid": ([ct.c_void_p, ct.c_double, ct.c_double, ct.c_double, ct.c_int, D], ct.c_double),
            "sil_config": ([D], None),
        }
        for name, (args, result) in signatures.items():
            fn = getattr(self.lib, name)
            fn.argtypes, fn.restype = args, result
        out = doubles([0] * 26)
        self.lib.sil_config(out)
        self.config = dict(zip(CONFIG_NAMES, out))
        if any(out[22:]):
            raise RuntimeError("Hardware output gates must remain false, including during SIL")

    def context(self, fixture, permit=True):
        return Context(self, fixture, permit)

    def encode(self, values, seq, flags=0, session=12345):
        packet = (ct.c_uint8 * 40)()
        self.lib.sil_encode(doubles(values), seq & 0xffffffff, session, flags, packet)
        return bytes(packet)

    def mix(self, values):
        out = doubles([0] * 10)
        self.lib.sil_mix(doubles(values), out)
        return list(out)

    def filter(self, tau=None):
        return Filter(self, tau or self.config["tau"])


class Context:
    def __init__(self, library, fixture, permit):
        self.library = library
        self.lib = library.lib
        keys = "thumb_open thumb_bent index_open index_bent pot_low pot_high".split()
        self.ptr = self.lib.sil_create(doubles([fixture[k] for k in keys]), permit)
        if not self.ptr:
            raise MemoryError("native context allocation")

    def close(self):
        if self.ptr:
            self.lib.sil_destroy(self.ptr)
            self.ptr = None

    def __enter__(self): return self
    def __exit__(self, *_): self.close()

    def glove(self, sample, adc, dt, ms, fresh, state, fault=0):
        out = doubles([0] * 19)
        packet = (ct.c_uint8 * 40)()
        tx = self.lib.sil_glove(self.ptr, doubles(sample), doubles(adc), dt, ms & 0xffffffff,
                                fault, fresh, state, packet, out)
        return list(out), bytes(packet) if tx else None

    def receive(self, packet, ms):
        buf = (ct.c_uint8 * len(packet)).from_buffer_copy(packet)
        return bool(self.lib.sil_receive(self.ptr, buf, len(packet), ms & 0xffffffff))

    def drone(self, sample, dt, ms, fault=0):
        out = doubles([0] * 48)
        self.lib.sil_drone(self.ptr, doubles(sample), dt, ms & 0xffffffff, fault, out)
        return list(out)

    def analog(self, adc, dt=.004):
        out = doubles([0] * 9)
        self.lib.sil_analog(self.ptr, doubles(adc), dt, out)
        return list(out)

    def pid(self, target, measured, dt=.004, freeze=False):
        out = doubles([0, 0])
        value = self.lib.sil_pid(self.ptr, target, measured, dt, freeze, out)
        return value, list(out)


class Filter:
    def __init__(self, library, tau):
        self.lib = library.lib
        self.ptr = self.lib.sil_filter_create(tau)

    def update(self, sample, dt):
        out = doubles([0] * 3)
        ok = self.lib.sil_filter(self.ptr, doubles(sample), dt, out)
        return bool(ok), list(out)

    def close(self):
        if self.ptr:
            self.lib.sil_filter_destroy(self.ptr)
            self.ptr = None

    def __enter__(self): return self
    def __exit__(self, *_): self.close()
