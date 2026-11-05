"""Read-only BNO055/PID shadow dashboard. Requires no third-party GUI package."""
import argparse
from collections import deque
import queue
import socket
import threading
import time
import tkinter as tk
from .shadow_codec import decode


def reader(port, out, stop):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("0.0.0.0", port))
        sock.settimeout(.2)
        while not stop.is_set():
            try:
                packet, _ = sock.recvfrom(512)
                row = decode(packet)
            except socket.timeout:
                continue
            except ValueError:
                continue
            try:
                out.put_nowait((time.monotonic(), row))
            except queue.Full:
                try: out.get_nowait()
                except queue.Empty: pass
                out.put_nowait((time.monotonic(), row))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=14550)
    args = parser.parse_args()
    root = tk.Tk()
    root.title("F450 BNO055 PID Shadow — ESC outputs disabled")
    root.geometry("1100x750")
    root.configure(bg="#111923")
    bold = ("Consolas", 13, "bold")
    normal = ("Consolas", 10)
    def label(parent, text, font=normal, color="#dce7f4"):
        return tk.Label(parent,text=text,font=font,fg=color,bg="#111923",anchor="w")
    title = label(root,"F450  /  BNO055 PID SHADOW  /  ESC OUTPUTS DISABLED",bold,"#60d7e5")
    title.pack(fill="x",padx=20,pady=12)
    status = label(root,"Waiting for UDP telemetry",bold,"#f5c36a")
    status.pack(fill="x",padx=20)
    row_top = tk.Frame(root,bg="#111923")
    row_top.pack(fill="x",padx=20,pady=10)
    orientation = label(row_top,"Sensor-frame attitude: unavailable",bold)
    orientation.pack(fill="x")
    calibration = label(row_top,"Calibration / IMU / timing: unavailable")
    calibration.pack(fill="x")
    pid = {}
    for axis in ("roll","pitch","yaw"):
        pid[axis] = label(row_top,f"{axis.upper()}: unavailable")
        pid[axis].pack(fill="x")
    motor_frame = tk.Frame(root,bg="#111923")
    motor_frame.pack(fill="x",padx=20,pady=10)
    bars = []
    for n in range(4):
        frame = tk.Frame(motor_frame,bg="#111923")
        frame.pack(side="left",expand=True,fill="x",padx=5)
        label(frame,f"M{n+1} calculated",normal).pack()
        canvas = tk.Canvas(frame,width=170,height=44,bg="#243445",highlightthickness=0)
        canvas.pack()
        bars.append(canvas)
    label(root,"Calculated motor commands only • ESC PWM unavailable • physical motor state unknown",normal,"#f5c36a").pack(fill="x",padx=20)
    graph = tk.Canvas(root,height=280,bg="#172433",highlightthickness=0)
    graph.pack(fill="both",expand=True,padx=20,pady=15)
    label(root,"Roll: cyan measured / dashed target    Pitch: orange measured / dashed target",normal).pack(fill="x",padx=20)
    controls = tk.Frame(root,bg="#111923")
    controls.pack(fill="x",padx=20,pady=12)
    tk.Button(controls,text="Motor test LOCKED",state="disabled").pack(side="left")
    tk.Button(controls,text="Close monitor",command=root.destroy).pack(side="right")
    inbox = queue.Queue(maxsize=8)
    stop = threading.Event()
    thread = threading.Thread(target=reader,args=(args.port,inbox,stop),daemon=True)
    thread.start()
    history = deque(maxlen=240)
    last = None
    def refresh():
        nonlocal last
        while True:
            try: last, data = inbox.get_nowait()
            except queue.Empty: break
            history.append(data)
            status.config(text=f"UDP LIVE • {data['mode']} • sequence {data['sequence']} • sensor valid {data['sensor_valid']}",fg="#74e6ac")
            orientation.config(text=f"Sensor frame  R {data['axes']['roll']['measured']:+7.2f}°   P {data['axes']['pitch']['measured']:+7.2f}°   Y relative {data['axes']['yaw']['measured']:+7.2f}°")
            calibration.config(text=f"Calibration 0x{data['calibration']:02X}   IMU errors {data['imu_errors']}   PID dt {data['dt_us']} µs   ESC DISABLED")
            for name in pid:
                a = data['axes'][name]
                pid[name].config(text=f"{name.upper():5} target {a['setpoint']:+6.2f}  measured {a['measured']:+7.2f}  error {a['error']:+7.2f}  P {a['p']:+.5f}  I {a['i']:+.5f}  D {a['d']:+.5f}  out {a['output']:+.5f}")
            for canvas,value in zip(bars,data['calculated_motor']):
                canvas.delete("all")
                canvas.create_rectangle(0,0,170*value,44,fill="#60d7e5",outline="")
                canvas.create_text(85,22,text=f"{value:.3f}",fill="white",font=bold)
        if last is None or time.monotonic()-last>1:
            status.config(text="TELEMETRY STALE / OFFLINE • ESC DISABLED",fg="#ff7777")
        graph.delete("all")
        width=max(graph.winfo_width(),100);height=max(graph.winfo_height(),100)
        graph.create_line(0,height/2,width,height/2,fill="#43586c")
        for axis,color in (("roll","#60d7e5"),("pitch","#efb566")):
            for key,dash in (("measured",None),("setpoint",(4,3))):
                points=[]
                for n,row in enumerate(history):
                    x=n*width/max(len(history)-1,1)
                    y=height/2-row['axes'][axis][key]*height/90
                    points.extend((x,y))
                if len(points)>=4:graph.create_line(*points,fill=color,width=2,dash=dash)
        root.after(50,refresh)
    def close():
        stop.set()
        root.destroy()
    root.protocol("WM_DELETE_WINDOW",close)
    refresh()
    root.mainloop()


if __name__ == "__main__": main()
