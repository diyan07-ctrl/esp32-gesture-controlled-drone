"""Read-only UDP telemetry console/logger for the motor-free drone bench image."""
import argparse
import json
import socket
import time
from pathlib import Path
from .telemetry_codec import decode, TelemetryError


class Tracker:
    def __init__(self, stale_s=1.0):
        self.stale_s=stale_s
        self.last_valid=None
        self.last_sequence=None
        self.last_uptime=None
        self.valid=self.invalid=self.missing=0
        self.intervals=[]

    def ingest(self,payload,received):
        try:
            packet=decode(payload)
        except TelemetryError:
            self.invalid+=1
            raise
        if self.last_sequence is not None:
            delta=(packet["sequence"]-self.last_sequence)&0xffffffff
            if self.last_uptime is not None and packet["uptime_ms"]<self.last_uptime:
                self.last_sequence=None # Firmware restarted; do not count wrap as packet loss.
            elif delta==0 or delta>=0x80000000:
                self.invalid+=1
                raise TelemetryError("duplicate or reordered sequence")
            else:
                self.missing+=delta-1
        if self.last_valid is not None and self.last_sequence is not None:
            self.intervals.append(received-self.last_valid)
            if len(self.intervals)>100:self.intervals.pop(0)
        self.last_valid=received
        self.last_sequence=packet["sequence"]
        self.last_uptime=packet["uptime_ms"]
        self.valid+=1
        packet["receive_monotonic_s"]=received
        packet["estimated_receive_rate_hz"]=(1/(sum(self.intervals)/len(self.intervals))
              if self.intervals and sum(self.intervals)>0 else None)
        packet["listener_missing_sequences"]=self.missing
        return packet

    def condition(self,now):
        if self.last_valid is None:return "NO_PACKET"
        if now-self.last_valid>self.stale_s:return "STALE_TELEMETRY_OR_NETWORK_LOSS"
        return "VALID_TELEMETRY"


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind",default="0.0.0.0")
    parser.add_argument("--port",type=int,default=14550)
    parser.add_argument("--stale-ms",type=int,default=1000)
    parser.add_argument("--duration",type=float,help="Stop after this many wall-clock seconds")
    parser.add_argument("--jsonl",type=Path,help="Append validated packets as JSON lines")
    parser.add_argument("--require-packets",action="store_true",help="Exit 2 if no valid datagram arrives")
    args=parser.parse_args(argv)
    tracker=Tracker(args.stale_ms/1000)
    output=None
    if args.jsonl:
        args.jsonl.parent.mkdir(parents=True,exist_ok=True)
        output=args.jsonl.open("a",encoding="utf-8")
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    sock.bind((args.bind,args.port));sock.settimeout(.2)
    print(f"UDP LISTEN {args.bind}:{args.port}; read-only; waiting for actual datagrams",flush=True)
    start=time.monotonic();previous_condition=None
    try:
        while args.duration is None or time.monotonic()-start<args.duration:
            try:payload,address=sock.recvfrom(512)
            except socket.timeout:payload=None
            now=time.monotonic()
            if payload is not None:
                try:packet=tracker.ingest(payload,now)
                except TelemetryError as error:
                    print(f"INVALID from {address[0]}: {error}",flush=True)
                else:
                    packet["source_ip"]=address[0]
                    if output:output.write(json.dumps(packet)+"\n");output.flush()
                    print(f"VALID seq={packet['sequence']} state={packet['state']} "
                          f"roll={packet['roll_deg']} pitch={packet['pitch_deg']} "
                          f"IMU={packet['imu_initialized']}/{packet['calibrated']} "
                          f"ESP-NOW={packet['espnow_initialized']}/{packet['peer_configured']} "
                          f"Wi-Fi={packet['wifi_connected']} age={packet['command_age_ms']}ms "
                          f"recv_rate={packet['estimated_receive_rate_hz']}Hz gaps={tracker.missing}",flush=True)
            condition=tracker.condition(now)
            if condition!=previous_condition:
                print("CONDITION "+condition,flush=True);previous_condition=condition
    except KeyboardInterrupt:pass
    finally:
        sock.close()
        if output:output.close()
    print(f"SUMMARY valid={tracker.valid} invalid={tracker.invalid} missing={tracker.missing}",flush=True)
    return 2 if args.require_packets and not tracker.valid else 0


if __name__=="__main__":raise SystemExit(main())
