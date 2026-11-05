"""Interactive, hardware-isolated SIL instrument cluster. Never opens serial/RF."""
import argparse
from collections import deque
import json
import math
from pathlib import Path
import queue
import shutil
import sys
import threading
import time
import numpy as np
from PySide6 import QtCore, QtGui, QtWidgets
import pyqtgraph as pg
import pyqtgraph.opengl as gl
from .build_native import ROOT
from .engine import Simulation, SCENARIOS, load_config
from .models import rotation

STATE = ["BOOT","INITIALIZING","CALIBRATION","DISARMED","ARMING CHECKS","ARMED (VIRTUAL)","FAILSAFE","FAULT"]
CYAN, AMBER = "#34d5ed", "#f4b453"


class Worker(QtCore.QThread):
    failed = QtCore.Signal(str)
    def __init__(self, config, scenario, snapshots, speed=1):
        super().__init__()
        self.config, self.scenario, self.snapshots, self.speed = config, scenario, snapshots, speed
        self.play = threading.Event()
        self.stop_requested = threading.Event()
        self.manual = None

    def run(self):
        sim = None
        try:
            sim = Simulation(self.config,self.scenario)
            deadline=time.perf_counter()
            while not self.stop_requested.is_set():
                if not self.play.is_set():
                    time.sleep(.02);deadline=time.perf_counter();continue
                snapshot=sim.step(self.manual)
                if sim.steps%10==0:
                    try:self.snapshots.get_nowait()
                    except queue.Empty:pass
                    self.snapshots.put_nowait(snapshot)
                deadline += self.config["dt"]/self.speed
                delay=deadline-time.perf_counter()
                if delay>0:time.sleep(min(delay,.01))
                elif delay<-.5:deadline=time.perf_counter()
        except Exception as error:
            self.failed.emit(str(error))
        finally:
            if sim is not None:sim.close()


class DroneView(gl.GLViewWidget):
    def __init__(self):
        super().__init__()
        self.setBackgroundColor("#0b131e")
        self.setMinimumHeight(320)
        self.setCameraPosition(distance=3.4,elevation=24,azimuth=-40)
        self.opts["center"]=QtGui.QVector3D(0,0,.9)
        grid=gl.GLGridItem();grid.setSize(10,10);grid.setSpacing(.25,.25);self.addItem(grid)
        axes=gl.GLAxisItem();axes.setSize(.5,.5,.5);self.addItem(axes)
        self.items_body=[]
        a=.45/(2*math.sqrt(2))
        mounts=np.array([[a,a,0],[a,-a,0],[-a,-a,0],[-a,a,0]])
        for i,pos in enumerate(mounts):
            color=(.2,.84,.93,1) if i<2 else (.96,.58,.22,1)
            arm=gl.GLLinePlotItem(pos=np.array([[0,0,0],pos]),color=color,width=9,antialias=True)
            self.addItem(arm);self.items_body.append(arm)
            # Cylinders and discs are geometric F450 motor/prop illustrations; no measured RPM inference.
            motor=gl.MeshData.cylinder(rows=2,cols=20,radius=[.022,.022],length=.035)
            vertices=motor.vertexes().copy()+pos+[0,0,-.04]
            mesh=gl.GLMeshItem(vertexes=vertices,faces=motor.faces(),color=(.65,.72,.8,1),smooth=True,shader="shaded")
            self.addItem(mesh);self.items_body.append(mesh)
            angles=np.linspace(0,2*np.pi,48)
            disc=pos+np.column_stack((.095*np.cos(angles),.095*np.sin(angles),np.full(48,-.055)))
            ring=gl.GLLinePlotItem(pos=disc,color=(*color[:3],.5),width=1.5,antialias=True)
            self.addItem(ring);self.items_body.append(ring)
            blade=gl.GLLinePlotItem(pos=np.array([pos+[-.09,0,-.055],pos+[.09,0,-.055]]),color=color,width=4,antialias=True)
            self.addItem(blade);self.items_body.append(blade)
        # Center plate: two triangles in body FRD coordinates.
        vertices=np.array([[-.06,-.045,-.012],[.06,-.045,-.012],[.06,.045,-.012],[-.06,.045,-.012]])
        plate=gl.GLMeshItem(vertexes=vertices,faces=np.array([[0,1,2],[0,2,3]]),color=(.35,.41,.5,1),smooth=False)
        self.addItem(plate);self.items_body.append(plate)
        nose=gl.GLLinePlotItem(pos=np.array([[0,0,-.025],[.1,0,-.025]]),color=(1,1,1,1),width=5)
        self.addItem(nose);self.items_body.append(nose)

    def attitude(self, q, position):
        c=np.diag([1,-1,-1])
        matrix=np.eye(4);matrix[:3,:3]=c@rotation(q);matrix[:3,3]=c@position
        transform=QtGui.QMatrix4x4(*matrix.flatten().tolist())
        for item in self.items_body:item.setTransform(transform)

    def reset_camera(self):
        self.opts["center"]=QtGui.QVector3D(0,0,.9)
        self.setCameraPosition(distance=3.4,elevation=24,azimuth=-40)


class Window(QtWidgets.QMainWindow):
    def __init__(self, config=None, scenario="hover", speed=1, autostart=False):
        super().__init__()
        self.config=config or load_config();self.scenario=scenario;self.speed=speed
        self.snapshots=queue.Queue(maxsize=1);self.history=deque(maxlen=750)
        self.last=None;self.worker=None;self.retired=[];self.capture_path=None
        self.report_process=None;self.report_folder=ROOT/"build/sil/dashboard-report"
        self.setWindowTitle("F450 SIL Lab | simulated data only | no hardware interfaces")
        self.resize(1640,1050)
        self.setStyleSheet("""
        QMainWindow,QWidget {background:#101925;color:#dbe7f1;font-family:'Segoe UI';font-size:12px;}
        QGroupBox {border:1px solid #28394d;border-radius:8px;margin-top:14px;padding:10px;}
        QGroupBox::title {subcontrol-origin:margin;left:12px;color:#829ab2;font-weight:bold;}
        QPushButton,QComboBox {background:#1d2d40;border:1px solid #34506c;border-radius:5px;padding:7px;}
        QPushButton:hover {border-color:#34d5ed;} QPushButton:disabled {color:#55677a;}
        QProgressBar {background:#192738;border:0;border-radius:3px;height:7px;}
        QProgressBar::chunk {background:#34d5ed;border-radius:3px;}
        QTabWidget::pane {border:1px solid #28394d;}
        QTabBar::tab {background:#162333;padding:8px 15px;} QTabBar::tab:selected {color:#34d5ed;}
        QSlider::groove:horizontal {background:#283e53;height:5px;}
        QSlider::handle:horizontal {background:#34d5ed;width:12px;margin:-5px 0;border-radius:5px;}
        """)
        root=QtWidgets.QWidget();self.setCentralWidget(root);layout=QtWidgets.QVBoxLayout(root)
        header=QtWidgets.QHBoxLayout()
        title=QtWidgets.QLabel("F450  /  SIL LAB");title.setStyleSheet("font-size:25px;font-weight:700;color:#f2f8fc;")
        header.addWidget(title);header.addStretch()
        self.status=QtWidgets.QLabel("SIMULATED DATA  |  VIRTUAL ACTUATORS ONLY")
        self.status.setStyleSheet("color:#f4b453;font-weight:bold;");header.addWidget(self.status);layout.addLayout(header)
        hint=QtWidgets.QLabel("Native ESP32 algorithms · FRD / NED · assumed motor physics · physical flight outputs remain disabled")
        hint.setStyleSheet("color:#829ab2;");layout.addWidget(hint)
        cards=QtWidgets.QHBoxLayout();self.cards={}
        for name in ("ROLL", "PITCH", "ALTITUDE", "TOTAL SIM THRUST", "SUPERVISOR"):
            group=QtWidgets.QGroupBox(name);box=QtWidgets.QVBoxLayout(group)
            label=QtWidgets.QLabel("--");label.setStyleSheet("font-size:25px;font-weight:600;color:#34d5ed;")
            box.addWidget(label);cards.addWidget(group);self.cards[name]=label
        layout.addLayout(cards)
        split=QtWidgets.QHBoxLayout();layout.addLayout(split,1)
        left=QtWidgets.QWidget();left.setFixedWidth(255);left_layout=QtWidgets.QVBoxLayout(left);left_layout.setContentsMargins(0,0,5,0)
        self.selector=QtWidgets.QComboBox();self.selector.addItems(SCENARIOS);self.selector.setCurrentText(scenario)
        left_layout.addWidget(self.selector)
        self.clock=QtWidgets.QLabel("t = 0.000 s   /   PAUSED");left_layout.addWidget(self.clock)
        buttons=QtWidgets.QGridLayout()
        for i,(text,slot) in enumerate((("Start",self.start),("Pause",self.pause),("Resume",self.start),("Reset",self.reset))):
            b=QtWidgets.QPushButton(text);b.clicked.connect(slot);buttons.addWidget(b,i//2,i%2)
        left_layout.addLayout(buttons)
        stop=QtWidgets.QPushButton("EMERGENCY SIM STOP");stop.setStyleSheet("background:#762d36;color:#fff;border-color:#c45d65;")
        stop.clicked.connect(self.emergency_stop);left_layout.addWidget(stop)
        reset_camera=QtWidgets.QPushButton("Reset camera");reset_camera.clicked.connect(lambda:self.view.reset_camera());left_layout.addWidget(reset_camera)
        self.follow=QtWidgets.QCheckBox("Follow simulated position");self.follow.setChecked(True);left_layout.addWidget(self.follow)
        input_group=QtWidgets.QGroupBox("VIRTUAL GLOVE");inputs=QtWidgets.QVBoxLayout(input_group)
        self.manual=QtWidgets.QCheckBox("Manual after scripted arm");inputs.addWidget(self.manual)
        self.sliders=[];self.slider_labels=[]
        for name,minimum,maximum,value in [("Hand roll",-45,45,0),("Hand pitch",-45,45,0),
                    ("Potentiometer %",0,100,78),("Thumb bend %",0,100,0),("Index bend %",0,100,0)]:
            label=QtWidgets.QLabel(f"{name}: {value}");inputs.addWidget(label)
            slider=QtWidgets.QSlider(QtCore.Qt.Horizontal);slider.setRange(minimum,maximum);slider.setValue(value)
            slider.valueChanged.connect(lambda v,l=label,n=name:l.setText(f"{n}: {v}"))
            inputs.addWidget(slider);self.sliders.append(slider)
        notes=QtWidgets.QLabel("Scripted calibration + low-pot arm\n0–6.8 s; support fixture until 9.5 s.\nManual controls start after 6.8 s.\nNo altitude / position hold.")
        notes.setStyleSheet("color:#829ab2;");inputs.addWidget(notes);left_layout.addWidget(input_group)
        self.speed_box=QtWidgets.QComboBox();self.speed_box.addItems(["1x","5x","20x"])
        self.speed_box.setCurrentText(f"{speed}x");left_layout.addWidget(self.speed_box)
        for text,slot in [("Run selected test",lambda:self.run_tests(False)),("Run all tests",lambda:self.run_tests(True)),
                          ("Export test report",self.export_report),("Save configuration",self.save_config)]:
            b=QtWidgets.QPushButton(text);b.clicked.connect(slot);left_layout.addWidget(b)
        self.test_status=QtWidgets.QLabel("Tests: not run in this window");self.test_status.setWordWrap(True);left_layout.addWidget(self.test_status)
        left_layout.addStretch();split.addWidget(left)
        center=QtWidgets.QVBoxLayout();split.addLayout(center,1)
        self.view=DroneView();center.addWidget(self.view,3)
        self.scene_label=QtWidgets.QLabel("Scene: north / west / up. Cyan arms = front; amber = rear. Mouse: orbit / zoom.")
        self.scene_label.setStyleSheet("color:#829ab2;");center.addWidget(self.scene_label)
        motors=QtWidgets.QHBoxLayout();self.motor_labels=[];self.motor_bars=[]
        for i,pos in enumerate(("FR / CCW","FL / CW","RL / CCW","RR / CW")):
            group=QtWidgets.QGroupBox(f"M{i+1}  {pos}");box=QtWidgets.QVBoxLayout(group)
            label=QtWidgets.QLabel("PWM -- us\nCommand -- %\nSim RPM --   |   Sim thrust -- N")
            bar=QtWidgets.QProgressBar();bar.setRange(0,1000);bar.setTextVisible(False)
            box.addWidget(label);box.addWidget(bar);motors.addWidget(group);self.motor_labels.append(label);self.motor_bars.append(bar)
        center.addLayout(motors)
        self.tabs=QtWidgets.QTabWidget();self.tabs.setMinimumHeight(290);center.addWidget(self.tabs,2)
        pg.setConfigOptions(antialias=True,background="#101925",foreground="#8fa6bc")
        self.plots={}
        self.add_plot_tab("Attitude / motion",[("roll","Roll (deg)",3),("pitch","Pitch (deg)",3),
                         ("yaw_rate","Yaw rate (deg/s)",1),("altitude","Altitude / vertical velocity",2)])
        self.add_plot_tab("Motors",[("motors","Normalized commands",4),("thrust","Simulated thrust (N)",4)])
        self.add_plot_tab("IMU / control",[("accel","Accelerometer (g)",3),("gyro","Gyroscope (deg/s)",3),
                         ("pid_error","Rate PID error (deg/s)",3),("latency","Simulated delivery latency (ms)",1)])
        right=QtWidgets.QWidget();right.setFixedWidth(295);rl=QtWidgets.QVBoxLayout(right);rl.setContentsMargins(5,0,0,0)
        self.imu_text=self.panel(rl,"IMU / ESTIMATION")
        self.pid_text=self.panel(rl,"NATIVE RATE PID: R / P / Y")
        self.link_text=self.panel(rl,"COMMUNICATION / TIMING")
        footer=QtWidgets.QLabel("SIL evidence reduces software risk.\nSimulated stability is not flight readiness.\nYaw is relative; thrust is model-estimated.")
        footer.setStyleSheet("color:#f4b453;");footer.setWordWrap(True);rl.addWidget(footer);rl.addStretch();split.addWidget(right)
        self.timer=QtCore.QTimer(self);self.timer.timeout.connect(self.refresh);self.timer.start(100)
        self.new_worker()
        if autostart:self.start()

    def panel(self,layout,title):
        group=QtWidgets.QGroupBox(title);box=QtWidgets.QVBoxLayout(group)
        label=QtWidgets.QLabel("Waiting for simulated samples");label.setWordWrap(True)
        label.setStyleSheet("font-family:Consolas;font-size:11px;");box.addWidget(label);layout.addWidget(group)
        return label

    def add_plot_tab(self,title,definitions):
        widget=QtWidgets.QWidget();grid=QtWidgets.QGridLayout(widget)
        colors=[CYAN,AMBER,"#bd9cff","#65d7a3"]
        for i,(key,label,count) in enumerate(definitions):
            plot=pg.PlotWidget(title=label);plot.showGrid(x=True,y=True,alpha=.15)
            curves=[plot.plot(pen=pg.mkPen(colors[j],width=1.5,style=QtCore.Qt.DashLine if key in ("roll","pitch") and j==2 else QtCore.Qt.SolidLine)) for j in range(count)]
            grid.addWidget(plot,i//2,i%2);self.plots[key]=curves
        self.tabs.addTab(widget,title)

    def new_worker(self):
        self.worker=Worker(self.config,self.selector.currentText(),self.snapshots,int(self.speed_box.currentText()[:-1]))
        self.worker.failed.connect(self.worker_failed);self.worker.start()

    def worker_failed(self,message):
        self.status.setText("WORKER ERROR: "+message);self.test_status.setText(message)
        if self.capture_path:
            print("Dashboard capture failed: "+message,file=sys.stderr,flush=True)
            QtWidgets.QApplication.instance().exit(2)

    def start(self):
        if not self.worker or not self.worker.isRunning():self.new_worker()
        self.worker.play.set()

    def pause(self):
        if self.worker:self.worker.play.clear()

    def emergency_stop(self):
        if self.worker:self.worker.stop_requested.set()
        self.status.setText("EMERGENCY SIM STOP / worker stopped / displayed values frozen")

    def reset(self):
        self.history.clear();self.last=None
        while not self.snapshots.empty():
            try:self.snapshots.get_nowait()
            except queue.Empty:break
        if self.worker and self.worker.isRunning():
            old=self.worker;self.retired.append(old)
            old.finished.connect(self.new_worker);old.stop_requested.set()
        else:self.new_worker()
        self.view.attitude([1,0,0,0],[0,0,-2]);self.status.setText("RESET / PAUSED / SIMULATION ONLY")

    def refresh(self):
        if self.worker:
            self.worker.speed=int(self.speed_box.currentText()[:-1])
            self.worker.manual=([self.sliders[0].value(),self.sliders[1].value()]+[s.value()/100 for s in self.sliders[2:]]) if self.manual.isChecked() else None
        try:s=self.snapshots.get_nowait()
        except queue.Empty:return
        self.last=s;self.history.append(s)
        self.view.attitude(s["q"],np.array(s["position"]))
        if self.follow.isChecked():
            self.view.opts["center"]=QtGui.QVector3D(s["position"][0],-s["position"][1],-s["position"][2])
        self.scene_label.setText(f"Position N/E/D: {s['position'][0]:+.2f} / {s['position'][1]:+.2f} / {s['position'][2]:+.2f} m · Cyan = front · Mouse orbit / zoom")
        self.cards["ROLL"].setText(f"{s['truth'][0]:+.2f}°")
        self.cards["PITCH"].setText(f"{s['truth'][1]:+.2f}°")
        self.cards["ALTITUDE"].setText(f"{-s['position'][2]:.2f} m")
        self.cards["TOTAL SIM THRUST"].setText(f"{sum(s['thrust']):.2f} N")
        self.cards["SUPERVISOR"].setText(STATE[s["state"]])
        self.cards["SUPERVISOR"].setStyleSheet(f"font-size:17px;font-weight:bold;color:{AMBER if s['state'] in (6,7) else CYAN};")
        self.clock.setText(f"t = {s['t']:.3f} s / {'RUNNING' if self.worker.play.is_set() else 'PAUSED'}")
        for i,(label,bar) in enumerate(zip(self.motor_labels,self.motor_bars)):
            label.setText(f"PWM {s['pwm'][i]:.1f} us   |   cmd {100*s['motor'][i]:.1f}%\nSim RPM {s['rpm'][i]:.0f}\nSim thrust {s['thrust'][i]:.3f} N")
            bar.setValue(round(s["motor"][i]*1000))
        vector=lambda a:" / ".join(f"{v:+.2f}" for v in a)
        imu=s["imu"];raw=[math.degrees(math.atan2(imu[1],imu[2])),math.degrees(math.atan2(-imu[0],math.hypot(imu[1],imu[2])))]
        self.imu_text.setText(f"Accel g  {vector(imu[:3])}\nGyro dps {vector(imu[3:])}\nAccel tilt {vector(raw)}\nFilter deg {vector(s['estimate'])}\nTruth deg  {vector(s['truth'])}\nError deg  {vector(np.array(s['estimate'])-s['truth'])}\nHealthy {s['imu_health']} / cal {s['calibrated']}\nCalibration {s['cal_samples']}/1000\nPot ADC {s['adc'][2]:.0f} / filtered {s['glove'][12]:.3f}")
        terms=lambda a:" / ".join(f"{v:+.5f}" for v in a)
        self.pid_text.setText(f"Target rate {vector(s['rate_target'])}\nMeasured    {vector(imu[3:])}\nError       {vector(s['pid_error'])}\nP      {terms(s['pid_p'])}\nI      {terms(s['pid_i'])}\nD      {terms(s['pid_d'])}\nOutput {terms(s['pid'])}")
        self.link_text.setText(f"Sent {s['sent']} / delivered {s['delivered']}\nDropped {s['dropped']} / rejected {s['rejected']:.0f}\nLatency {s['latency_ms']:.2f} ms\nCommand age {s['age_ms']:.0f} ms\nFault bits {s['faults']}\nLoop dt {1000*s['loop_dt_s']:.3f} ms\nHost native {s['host_native_us']:.1f} us\nFixture {'ON' if s['fixture'] else 'RELEASED'}\nReturn status: idealized")
        rows=list(self.history);t=[r["t"] for r in rows]
        fields={"roll":[[r[k][0] for r in rows] for k in ("setpoint","truth","estimate")],
            "pitch":[[r[k][1] for r in rows] for k in ("setpoint","truth","estimate")],
            "yaw_rate":[[r["omega"][2] for r in rows]],
            "altitude":[[-r["position"][2] for r in rows],[-r["velocity"][2] for r in rows]],
            "latency":[[r["latency_ms"] for r in rows]]}
        for key,field,count in [("motors","motor",4),("thrust","thrust",4),("accel","imu",3),
                                 ("gyro","imu",3),("pid_error","pid_error",3)]:
            fields[key]=[[r[field][i+(3 if key=="gyro" else 0)] for r in rows] for i in range(count)]
        for key,curves in self.plots.items():
            for curve,values in zip(curves,fields[key]):curve.setData(t,values)
            minimum=min(min(v) for v in fields[key]);maximum=max(max(v) for v in fields[key])
            pad=max(.05,(maximum-minimum)*.1)
            curves[0].getViewBox().setRange(xRange=(min(t),max(t)+.01),yRange=(minimum-pad,maximum+pad),padding=0)
        if self.capture_path and s["t"]>=self.capture_time:
            self.pause();QtCore.QTimer.singleShot(400,self.capture)

    def run_tests(self,all_tests):
        if self.report_process and self.report_process.state()!=QtCore.QProcess.NotRunning:return
        self.report_process=QtCore.QProcess(self)
        args=["-m","simulation.validate","--no-build","--skip-host","--output",str(self.report_folder)]
        if not all_tests:args += ["--scenario",self.selector.currentText()]
        self.report_process.setWorkingDirectory(str(ROOT))
        self.report_process.setProcessChannelMode(QtCore.QProcess.MergedChannels)
        self.report_process.readyReadStandardOutput.connect(lambda:self.report_process.readAllStandardOutput())
        self.report_process.finished.connect(self.tests_finished)
        self.test_status.setText("Running tests/report in isolated child process…")
        self.report_process.start(sys.executable,args)

    def tests_finished(self,code,_status):
        path=self.report_folder/"results.json"
        if path.exists():
            r=json.loads(path.read_text());self.test_status.setText(f"Exit {code}: {r['status']}\n{r['unexpected_failures']} failures; {r['known_limitations']} limitations")
        else:self.test_status.setText(f"Report process failed, exit {code}; inspect terminal/environment")

    def export_report(self):
        if not (self.report_folder/"REPORT.md").exists():
            self.test_status.setText("Run tests first to generate an exportable report.");return
        target=QtWidgets.QFileDialog.getExistingDirectory(self,"Export report folder",str(ROOT/"build/sil"))
        if target:
            destination=Path(target)/"f450-sil-report"
            if destination.resolve()==self.report_folder.resolve():return
            shutil.copytree(self.report_folder,destination,dirs_exist_ok=True)
            self.test_status.setText(f"Exported {destination}")

    def save_config(self):
        path,_=QtWidgets.QFileDialog.getSaveFileName(self,"Save simulation-only configuration",str(ROOT/"build/sil/custom.json"),"JSON (*.json)")
        if path:Path(path).write_text(json.dumps(self.config,indent=2),encoding="utf-8")

    def capture(self):
        if not self.capture_path:return
        path=Path(self.capture_path);self.capture_path=None
        path.parent.mkdir(parents=True,exist_ok=True)
        diagnostics={"history_records":len(self.history),"worker_queue":self.snapshots.qsize(),
            "curves":{key:len(curves[0].getData()[0]) for key,curves in self.plots.items()},
            "roll_plot_range":self.plots["roll"][0].getViewBox().viewRange(),
            "roll_plot_data":[list(self.plots["roll"][0].getData()[0]),list(self.plots["roll"][0].getData()[1])],
            "last_snapshot":self.last,"opengl_valid":self.view.isValid()}
        if not diagnostics["opengl_valid"] or not all(diagnostics["curves"].values()):
            raise RuntimeError("Capture failed: OpenGL context or plot data unavailable")
        path.with_suffix(".json").write_text(json.dumps(diagnostics,indent=2),encoding="utf-8")
        self.grab().save(str(path))
        self.view.grabFramebuffer().save(str(path.with_name(path.stem+"-3d.png")))
        print(f"Captured actual Qt/OpenGL dashboard: {path}",flush=True)
        QtCore.QTimer.singleShot(200,self.close)

    def closeEvent(self,event):
        if self.worker:self.worker.stop_requested.set();self.worker.wait(2000)
        for worker in self.retired:worker.stop_requested.set();worker.wait(2000)
        if self.report_process and self.report_process.state()!=QtCore.QProcess.NotRunning:
            self.report_process.terminate();self.report_process.waitForFinished(2000)
        event.accept()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config");parser.add_argument("--scenario",choices=SCENARIOS,default="hover")
    parser.add_argument("--speed",type=int,choices=[1,5,20],default=1)
    parser.add_argument("--capture",help="Capture the actual rendered dashboard, then close")
    parser.add_argument("--capture-time",type=float,default=11.5,help="Simulation time for a dashboard capture")
    args=parser.parse_args()
    app=QtWidgets.QApplication(sys.argv[:1])
    window=Window(load_config(args.config),args.scenario,args.speed,bool(args.capture))
    window.capture_path=args.capture;window.capture_time=args.capture_time;window.show()
    # Bounded capture timeout reports errors instead of hanging a validation session.
    if args.capture:
        def capture_timeout():
            if window.capture_path:
                print("Dashboard capture timed out before the requested simulation time",file=sys.stderr,flush=True)
                app.exit(2)
        QtCore.QTimer.singleShot(45000,capture_timeout)
    return app.exec()


if __name__=="__main__":sys.exit(main())
