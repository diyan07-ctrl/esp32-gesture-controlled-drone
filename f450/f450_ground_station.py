"""F450 UDP pilot console. Python 3.10+, standard library only."""
import json
import math
import secrets
import socket
import time
import tkinter as tk
from tkinter import ttk

NAMES = ['Roll angle', 'Pitch angle', 'Roll rate', 'Pitch rate', 'Yaw rate', 'Altitude']


class Station:
    def __init__(self, root):
        self.root = root
        root.title('F450 pilot console')
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(('', 4211))
        self.sock.setblocking(False)
        self.seq = secrets.randbelow(2**31)
        self.keys = set()
        self.pending = {}
        self.last_rx = 0
        self.next_send = 0
        self.telemetry = {}
        self.session_id = None
        self.owns_session = False
        self.discovery_seq = None
        self.hello_seq = None
        self.throttle = tk.DoubleVar(value=1180)
        self.status = tk.StringVar(value='Connect Wi-Fi to F450_DRONE')
        self.ack = tk.StringVar(value='Waiting for configuration')
        self.live = tk.StringVar(value='No telemetry')
        ttk.Label(root, textvariable=self.status).grid(row=0, column=0, columnspan=7)
        ttk.Label(root, textvariable=self.live, justify='left').grid(row=1, column=0, columnspan=7)
        ttk.Label(root, text='W/S pitch • A/D roll • Q/E yaw • R/F climb • SPACE neutral • X stop').grid(row=2, column=0, columnspan=7)
        bar = ttk.Frame(root)
        bar.grid(row=3, column=0, columnspan=7)
        ttk.Button(bar,text='Connect / resume',command=self.acquire).pack(side='left')
        for text, kind in [('ARM', 'ARM'), ('DISARM / STOP', 'STOP'), ('Release ownership','RELEASE'), ('Clear recoverable failsafe', 'CLEAR_FAULT'), ('Query gains', 'GET'), ('Reset reference defaults', 'RESET'), ('Save gains to NVS', 'SAVE'), ('Manual throttle', 'MANUAL')]:
            ttk.Button(bar, text=text, command=lambda k=kind: self.send(k)).pack(side='left')
        self.entries = []
        self.current = []
        for col, title in enumerate(['Controller', 'Current Kp / Ki / Kd', 'Kp', 'Ki', 'Kd']):
            ttk.Label(root, text=title).grid(row=4, column=col)
        for row, name in enumerate(NAMES, 5):
            ttk.Label(root, text=name).grid(row=row, column=0)
            current = tk.StringVar(value='unknown')
            self.current.append(current)
            ttk.Label(root, textvariable=current).grid(row=row, column=1)
            fields = []
            for col in range(2, 5):
                frame = ttk.Frame(root)
                frame.grid(row=row, column=col)
                var = tk.StringVar(value='')
                fields.append(var)
                ttk.Button(frame, text='−', width=2, command=lambda v=var: self.adjust(v, -.01)).pack(side='left')
                ttk.Entry(frame, textvariable=var, width=9).pack(side='left')
                ttk.Button(frame, text='+', width=2, command=lambda v=var: self.adjust(v, .01)).pack(side='left')
            self.entries.append(fields)
        ttk.Button(root, text='Send all PID gains', command=self.apply).grid(row=11, column=0, columnspan=5)
        self.base = tk.StringVar(value='1180')
        self.target = tk.StringVar(value='30')
        ttk.Label(root, text='Base throttle µs (disarmed)').grid(row=12, column=0)
        ttk.Entry(root, textvariable=self.base).grid(row=12, column=1)
        ttk.Button(root, text='Set base', command=lambda: self.numeric('BASE', self.base, 1180, 1800)).grid(row=12, column=2)
        ttk.Label(root, text='Target frame height cm').grid(row=13, column=0)
        ttk.Entry(root, textvariable=self.target).grid(row=13, column=1)
        ttk.Button(root, text='Enable / set altitude hold', command=lambda: self.numeric('ALT', self.target, 6.03, 204.03)).grid(row=13, column=2)
        ttk.Label(root, textvariable=self.ack).grid(row=14, column=0, columnspan=5)
        ttk.Label(root, text='Manual collective us: Up/Down; ignored in ALTITUDE_HOLD').grid(row=15,column=0,columnspan=2)
        ttk.Scale(root,from_=1180,to=1800,variable=self.throttle,length=400).grid(row=15,column=2,columnspan=3)
        ttk.Label(root,textvariable=self.throttle).grid(row=16,column=0)
        ttk.Button(root,text='Confirm altitude gains bench-reviewed (disarmed)',command=lambda:self.send('ALT_VALIDATE')).grid(row=16,column=1,columnspan=3)
        root.bind('<KeyPress>', self.press)
        root.bind('<KeyRelease>', lambda e: self.keys.discard(e.keysym.lower()))
        root.bind('<FocusOut>', lambda e: self.keys.clear())
        root.protocol('WM_DELETE_WINDOW', self.close)
        self.acquire()
        self.tick()

    def acquire(self):
        if self.telemetry.get('armed'):
            self.ack.set('Disarm before reconnecting the command session')
            return
        self.pending.clear()
        self.owns_session = False
        self.hello_seq = None
        self.discovery_seq = self.send('GET')

    def send(self, kind, **values):
        if kind not in ('GET','HELLO','STOP','DISARM') and not self.owns_session:
            self.ack.set('Acquire ownership with Connect / resume first')
            return
        if kind in ('ARM', 'CLEAR_FAULT'):
            token = self.telemetry.get('arm_token')
            if type(token) is not int or not 0 <= token <= 0xffffffff:
                self.ack.set('Query current status before ARM or CLEAR_FAULT')
                return
            values['arm_token'] = token
        if kind in ('STOP', 'DISARM', 'RELEASE', 'ARM', 'CLEAR_FAULT'):
            self.telemetry.pop('arm_token', None)
        if kind == 'MANUAL' and hasattr(self, 'throttle'):
            self.throttle.set(self.telemetry.get('collective_us',1180))
        self.seq = (self.seq + 1) & 0xffffffff
        data = dict(type=kind, seq=self.seq, **values)
        if kind != 'GET' and self.session_id is not None:
            data['session'] = self.session_id
        try:
            self.sock.sendto(json.dumps(data, allow_nan=False).encode(), ('192.168.4.1', 4210))
            self.pending[self.seq] = (time.monotonic(), kind)
            return self.seq
        except (OSError, ValueError) as exc:
            self.ack.set(str(exc))

    @staticmethod
    def value(var, lo, hi):
        value = float(var.get())
        if not math.isfinite(value) or not lo <= value <= hi:
            raise ValueError(f'Enter a finite value between {lo} and {hi}')
        return value

    def adjust(self, var, delta):
        try:
            var.set(f'{max(0, min(100, self.value(var, 0, 100) + delta)):.4g}')
        except ValueError as exc:
            self.ack.set(str(exc))

    def numeric(self, kind, var, lo, hi):
        try:
            self.send(kind, value=self.value(var, lo, hi))
        except ValueError as exc:
            self.ack.set(str(exc))

    def apply(self):
        try:
            gains = [[self.value(v, 0, 100) for v in row] for row in self.entries]
            self.send('PID', gains=gains)
        except ValueError as exc:
            self.ack.set(str(exc))

    def press(self, event):
        key = event.keysym.lower()
        if key == 'x':
            self.keys.clear()
            self.throttle.set(1180)
            self.send('STOP')
        elif isinstance(self.root.focus_get(), (ttk.Entry, tk.Entry)):
            return
        elif key == 'space':
            self.keys.clear()
        else:
            self.keys.add(key)

    def receive(self, data):
        if not isinstance(data, dict):
            return
        self.last_rx = time.monotonic()
        if type(data.get('arm_token')) is int:
            self.telemetry['arm_token'] = data['arm_token']
        if data.get('type') == 'ACK':
            pending = self.pending.pop(data.get('seq'), None)
            self.ack.set(f"ACK {data.get('seq')}: {data.get('reason')}")
            if pending and data.get('seq') == self.discovery_seq and pending[1] == 'GET':
                self.discovery_seq = None
                if data.get('ok') and type(data.get('session')) is int:
                    if data.get('owner_active') and not data.get('owned'):
                        self.ack.set('Another IP owns the controller; owner must release while disarmed')
                    else:
                        self.session_id = data['session']
                        if data.get('owner_active') and data.get('seq_set'):
                            self.seq = data['last_seq']
                        self.hello_seq = self.send('HELLO')
            elif pending and data.get('seq') == self.hello_seq and pending[1] == 'HELLO':
                self.hello_seq = None
                self.owns_session = bool(data.get('ok') and data.get('owned') and data.get('session') == self.session_id)
            elif pending and pending[1] == 'RELEASE':
                if data.get('ok'):
                    self.owns_session = False
                    self.session_id = None
                    self.discovery_seq = self.hello_seq = None
                    self.pending.clear()
                elif 'owned' in data:
                    self.owns_session = bool(data['owned'] and data.get('session') == self.session_id)
            elif pending and 'owned' in data and (not data['owned'] or data.get('session') != self.session_id):
                self.owns_session = False
        if data.get('type') == 'TEL':
            self.telemetry = data
            self.live.set('\n'.join(f'{k}: {data.get(k)}' for k in ['state','mode','relative','gyro','height','target','requested_target','alt_error','outputs','collective_us','manual_throttle_us','motors','armed','arming_block_reason','reference_ready','reference_samples','imu_ok','range_ok','range_age_ms','range_rejected','alt_configured','alt_validated','battery_status','fault','watchdog','overruns']))
            if not data.get('armed'):
                self.throttle.set(1180)
        gains = data.get('gains')
        if isinstance(gains, list) and len(gains) == 6:
            for i, row in enumerate(gains):
                if not isinstance(row, list) or len(row) != 3:
                    continue
                self.current[i].set(' / '.join(map(str, row)))
                # Telemetry never overwrites edits. Explicit configuration responses do.
                for var, value in zip(self.entries[i], row):
                    if not var.get() or data.get('type') == 'ACK':
                        var.set(str(value))

    def tick(self):
        now = time.monotonic()
        if now >= self.next_send:
            self.next_send = now + .02
            k = self.keys
            if self.telemetry.get('mode') != 'ALTITUDE_HOLD':
                self.throttle.set(max(1180,min(1800,self.throttle.get()+2*(('up' in k)-('down' in k)))))
            if self.owns_session:
                self.send('CMD', roll=float(('d' in k)-('a' in k)), pitch=float(('s' in k)-('w' in k)), yaw=float(('e' in k)-('q' in k)), climb=float(('r' in k)-('f' in k)),throttle=float(self.throttle.get()))
        for _ in range(20):
            try:
                raw, source = self.sock.recvfrom(4096)
                if source[0] == '192.168.4.1':
                    self.receive(json.loads(raw))
            except BlockingIOError:
                break
            except (OSError, ValueError, TypeError):
                continue
        expired = [seq for seq, (sent, _) in self.pending.items() if now-sent > 1]
        for seq in expired:
            _, kind = self.pending.pop(seq)
            if kind == 'CMD':
                continue
            if kind in ('HELLO','RELEASE'):
                self.owns_session = False
            self.ack.set(f'{kind}: acknowledgement timed out; query status before retrying')
        self.status.set(('CONNECTED / OWNER' if self.owns_session else 'CONNECTED / NO CONFIRMED OWNERSHIP') if now-self.last_rx < .5 else 'TELEMETRY LOST — command timeout disarms ESP32')
        self.root.after(10, self.tick)

    def close(self):
        for _ in range(3):
            self.send('STOP')
        self.send('RELEASE')
        self.sock.close()
        self.root.destroy()


if __name__ == '__main__':
    window = tk.Tk()
    Station(window)
    window.mainloop()
