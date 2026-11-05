"""Standalone numerical report, plots and reviewable engineering findings."""
import csv
import hashlib
import json
import math
import platform
import subprocess
import sys
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from .build_native import ROOT, sources_hash
from .engine import Simulation, SCENARIOS, load_config
from .native import Library
from .experiments import (filter_experiment,scenario_metrics,network_experiment,
                          mixer_experiment,analog_experiment)


def clean(value):
    if isinstance(value,dict): return {str(k):clean(v) for k,v in value.items()}
    if isinstance(value,(list,tuple)):return [clean(v) for v in value]
    if isinstance(value,np.generic):return clean(value.item())
    if isinstance(value,float) and not math.isfinite(value): return None
    return value


def dump(path, value):
    path.write_text(json.dumps(clean(value),indent=2,allow_nan=False),encoding="utf-8")


def harness_hash():
    digest=hashlib.sha256()
    paths=sorted((ROOT/"simulation").glob("*.py"))
    paths += sorted((ROOT/"simulation/config").glob("*.json"))
    for path in paths:
        digest.update(path.relative_to(ROOT).as_posix().encode())
        digest.update(path.read_bytes())
    return digest.hexdigest()


def plot_filter(rows, name, path):
    fig,ax=plt.subplots(3,1,figsize=(11,8),sharex=True)
    for i,label in enumerate(("Roll", "Pitch")):
        ax[i].plot(rows[:,0],rows[:,1+i],label="Ground truth",color="#111827",lw=2)
        ax[i].plot(rows[:,0],rows[:,7+i],label="Accel only",alpha=.4)
        ax[i].plot(rows[:,0],rows[:,9+i],label="Gyro integrated",alpha=.65)
        ax[i].plot(rows[:,0],rows[:,4+i],label="Actual C++ complementary")
        ax[i].set_ylabel(label+" (deg)");ax[i].grid(alpha=.2)
    ax[0].legend(ncol=2)
    ax[2].plot(rows[:,0],rows[:,6],label="Relative yaw (no magnetic correction)")
    ax[2].set_ylabel("Yaw (deg)");ax[2].set_xlabel("Simulation time (s)");ax[2].grid(alpha=.2);ax[2].legend()
    fig.suptitle("SIMULATED MPU6050 / "+name);fig.tight_layout();fig.savefig(path,dpi=130);plt.close(fig)


def plot_scenario(rows,name,path):
    t=[r["t"] for r in rows]
    fig,ax=plt.subplots(4,1,figsize=(11,10),sharex=True)
    for i,color in [(0,"#0891b2"),(1,"#d97706")]:
        ax[0].plot(t,[r["truth"][i] for r in rows],color=color,label=("Roll","Pitch")[i]+" truth")
        ax[0].plot(t,[r["estimate"][i] for r in rows],color=color,ls="--",alpha=.6,label=("Roll","Pitch")[i]+" estimate")
        ax[0].plot(t,[r["setpoint"][i] for r in rows],color=color,ls=":")
    ax[0].set_ylabel("Attitude (deg)");ax[0].legend(ncol=2)
    ax[1].plot(t,[-r["position"][2] for r in rows],label="Altitude")
    ax[1].plot(t,[-r["velocity"][2] for r in rows],label="Vertical velocity (m/s)")
    ax[1].set_ylabel("m / m/s");ax[1].legend()
    for i in range(4):ax[2].plot(t,[r["motor"][i] for r in rows],label=f"M{i+1}")
    ax[2].set_ylabel("Command (0..1)");ax[2].legend(ncol=4)
    ax[3].plot(t,[sum(r["thrust"]) for r in rows],label="Total simulated thrust (N)")
    ax[3].plot(t,[r["state"] for r in rows],label="Supervisor state code")
    ax[3].set_xlabel("Simulation time (s)");ax[3].legend()
    for a in ax:a.grid(alpha=.2);a.axvline(9.5,color="gray",ls=":",alpha=.6)
    fig.suptitle("SIMULATION ONLY / "+name+" / fixture released at 9.5 s")
    fig.tight_layout();fig.savefig(path,dpi=130);plt.close(fig)


def generate(output, config_path=None, selected=None, baseline=None):
    output=Path(output).resolve();output.mkdir(parents=True,exist_ok=True)
    plots=output/"plots";plots.mkdir(exist_ok=True)
    config=load_config(config_path);library=Library()
    checks=[]
    def check(name,ok,detail,limitation=False):
        checks.append({"name":name,"status":"PASS" if ok else "LIMITATION" if limitation else "FAIL", "detail":detail})
    revision=subprocess.run(["git","-c",f"safe.directory={ROOT.as_posix()}","rev-parse","HEAD"],
        cwd=ROOT,capture_output=True,text=True,check=True).stdout.strip()
    dirty=subprocess.run(["git","-c",f"safe.directory={ROOT.as_posix()}","status","--porcelain"],
        cwd=ROOT,capture_output=True,text=True,check=True).stdout.strip()
    summary={"label":"ALL DATA SIMULATED; NO HARDWARE VALIDATION", "revision":revision,
        "working_tree_dirty":bool(dirty),"native_sources_sha256":sources_hash(),
        "harness_sources_sha256":harness_hash(),
        "environment":{"python":sys.version,"os":platform.platform(),"numpy":np.__version__,"matplotlib":matplotlib.__version__},
        "config":config,"firmware_config":library.config,"filters":[],"scenarios":[],"sensitivity":[]}
    filter_names=["stationary","gradual","sudden","noise","bias_drift","translation","invalid","jitter"]
    for tau in [.25,.5,1.0]:
        for name in filter_names:
            metric,rows=filter_experiment(library,config,name,tau)
            summary["filters"].append(metric)
            if tau==.5:
                plot_filter(rows,name,plots/f"filter_{name}.png")
                if name in ("stationary","gradual","noise"):
                    check("filter_"+name,metric["roll"]["rmse_deg"]<2 and metric["pitch"]["rmse_deg"]<2,
                          "Fixed 2 degree RMS expectation for synthetic gravity-only characterization")
                if name=="translation":
                    check("acceleration_is_not_tilt",metric["pitch"]["max_abs_deg"]<2,
                        "Six-axis norm gating cannot reject all non-gravity acceleration; no filter retuning conceals this",True)
    # Reproduce the pre-existing one-cycle valid flag beyond its documented pitch envelope.
    with library.filter() as filt:
        p=math.radians(59.9)
        ok,value=filt.update([-math.sin(p),0,math.cos(p),0,494,0],.004)
        check("estimator_pitch_envelope",not ok,"A 59.9-degree estimate plus high valid gyro rate must reject the sample that crosses 60 degrees")
        summary["envelope_probe"]={"accepted":ok,"pitch_deg":value[1]}
    summary["network"]=network_experiment(library,config)
    check("network_validation",150<summary["network"]["accepted"]<300 and 30<summary["network"]["rejected"]<150,
          "Seeded loss/latency/duplicate/reordering/corruption; meaningful delivery/validation bounds")
    summary["mixer"]=mixer_experiment(library)
    check("mixer_signs",np.allclose(summary["mixer"][2]["motors"],[.4,.6,.6,.4],atol=1e-6),"FR/FL/RL/RR roll allocation")
    summary["analog"],analog=analog_experiment(library,config)
    check("analog_hysteresis",summary["analog"]["hysteresis_switches_on_noisy_hold"]==0,"No bent-state chatter on noisy hold")
    fig,ax=plt.subplots(3,1,figsize=(10,7),sharex=True)
    ax[0].plot(analog[:,0],analog[:,3]);ax[0].set_ylabel("Pot ADC (synthetic)")
    ax[1].plot(analog[:,0],analog[:,11],label="Normalized target");ax[1].plot(analog[:,0],analog[:,12],label="Filtered");ax[1].legend()
    ax[2].plot(analog[:,0],analog[:,7],label="Thumb normalized");ax[2].plot(analog[:,0],analog[:,9],label="Bent state");ax[2].legend();ax[2].set_xlabel("Time (s)")
    fig.suptitle("Synthetic ADC fixtures / actual C++ calibration and filtering");fig.tight_layout();fig.savefig(plots/"analog.png",dpi=130);plt.close(fig)
    scenarios=selected or SCENARIOS
    for name in scenarios:
        sim=Simulation(config,name,library)
        try:
            rows=sim.run();metric=scenario_metrics(sim,rows);summary["scenarios"].append(metric)
            dump(output/f"trace_{name}.json",rows)
            plot_scenario(rows,name,plots/f"scenario_{name}.png")
            check(name+"_finite_bounds",metric["finite"] and 0<=metric["min_motor"]<=metric["max_motor"]<=1,
                  "Finite rigid body/motor state and bounded actual native command output")
            if name in ("wireless_loss","imu_failure","missed_deadline"):
                mask={"wireless_loss":8,"imu_failure":1,"missed_deadline":2}[name]
                check(name+"_inhibit",metric["final_state"] in (6,7) and bool(metric["final_faults"]&mask) and rows[-1]["pwm"]==[0]*4,
                    "Latched fault and virtual PWM inhibition; reconnection cannot rearm")
            elif name=="startup":check("startup_inhibited",metric["final_state"]==3 and rows[-1]["pwm"]==[0]*4,"No deliberate arm input")
            elif name in ("roll_step","pitch_step","combined_step"):
                check(name+"_settles",abs(metric["steady_state_error_deg"])<1 and metric["final_state"]==5,
                    "5-degree component step with gravity-only sensor characterization; not full translational accelerometer physics")
            elif name in ("hover","arming","disturbance","timing_jitter"):
                check(name+"_attitude",metric["roll"]["rmse_deg"]<2 and metric["pitch"]["rmse_deg"]<2 and metric["final_state"]==5,
                    "Attitude remains within 2 degree RMS; no altitude-hold requirement")
            if name in ("forward_backward","left_right"):
                axis=1 if name=="forward_backward" else 0
                early=next(r for r in rows if r["t"]>=10.5)
                late=next(r for r in rows if r["t"]>=13.0)
                check(name+"_signs",early["setpoint"][axis]<-10 and early["truth"][axis]<-5
                      and late["setpoint"][axis]>10 and late["truth"][axis]>5,
                      "Actual glove mapping, packet decode, PID/mix and plant respond in both signed directions")
                recovery=next(r for r in rows if r["t"]>=15)
                check(name+"_prompt_recovery",abs(recovery["truth"][axis])<2,
                      "After the glove returns neutral, assumed-plant attitude still exceeds 15 degrees at 15 s; recovery/translation require further control analysis",True)
            if name=="throttle":
                high=next(r for r in rows if r["t"]>=10.5)
                low=next(r for r in rows if r["t"]>=12.5)
                check("throttle_direction",high["glove"][13]>low["glove"][13]
                      and np.mean(high["motor"])>np.mean(low["motor"]),
                      "Potentiometer demand and actual native motor command follow high then low phase")
            if name=="fingers":
                thumb=next(r for r in rows if r["t"]>=10.5)
                index=next(r for r in rows if r["t"]>=12.5)
                check("finger_gestures",thumb["glove"][9]==1 and thumb["glove"][10]==0
                      and index["glove"][9]==0 and index["glove"][10]==1,
                      "Actual native flex pipeline distinguishes thumb-only and index-only")
        finally:sim.close()
    # Parameter uncertainty, kept separate from firmware tuning. Same 5-degree task.
    for key,multipliers in [("mass_kg",[.8,1.2]),("kt_N_s2",[.8,1.2]),("motor_tau_s",[.5,2])]:
        for multiplier in multipliers:
            modified=json.loads(json.dumps(config));modified["vehicle"][key]*=multiplier
            sim=Simulation(modified,"roll_step",library)
            try:
                rows=sim.run(16);m=scenario_metrics(sim,rows)
                summary["sensitivity"].append({"parameter":key,"multiplier":multiplier,
                    "steady_error_deg":m["steady_state_error_deg"],"overshoot_pct":m["overshoot_pct"],
                    "roll_rmse_deg":m["roll"]["rmse_deg"],"final_state":m["final_state"]})
            finally:sim.close()
    check("sender_generation_age",False,"Command v2 has no sender creation timestamp/challenge. Previously unseen delayed sequence can appear fresh on arrival; duplicates/old sequences are rejected",True)
    check("altitude_hold",False,"No altitude controller. Quantized open-loop hover throttle produces height drift; simulated thrust is not hardware thrust",True)
    check("stuck_pot_detection",False,"A plausible stuck ADC is indistinguishable from a deliberately held knob without independent sensing/interlock",True)
    summary["checks"]=checks
    summary["unexpected_failures"]=sum(c["status"]=="FAIL" for c in checks)
    summary["known_limitations"]=sum(c["status"]=="LIMITATION" for c in checks)
    summary["status"]="FAIL" if summary["unexpected_failures"] else "QUALIFIED: software regressions pass; engineering limitations remain"
    if baseline:
        old=json.loads(Path(baseline).read_text(encoding="utf-8"))
        summary["comparison"]={"before_revision":old["revision"],"before_sources_sha256":old["native_sources_sha256"],
            "before_unexpected_failures":old["unexpected_failures"],"after_unexpected_failures":summary["unexpected_failures"],
            "before_envelope":old["envelope_probe"],"after_envelope":summary["envelope_probe"]}
    dump(output/"results.json",summary)
    write_markdown(summary,output/"REPORT.md")
    print(json.dumps({"status":summary["status"],"failures":summary["unexpected_failures"],
        "limitations":summary["known_limitations"],"report":str(output/"REPORT.md")},indent=2))
    return summary


def write_markdown(s,path):
    lines=["# Deterministic F450 SIL engineering report", "", "**ALL DATA SIMULATED. No hardware or flight validation.**", "",
        f"Result: {s['status']}",f"Revision: `{s['revision']}`; dirty source at generation: {s['working_tree_dirty']}.",
        f"Native source fingerprint: `{s['native_sources_sha256']}`.","", "## Environment and model", "",
        f"Harness source fingerprint: `{s['harness_sources_sha256']}`.",
        f"Python: {s['environment']['python'].splitlines()[0]}; NumPy {s['environment']['numpy']}; Matplotlib {s['environment']['matplotlib']}.",
        "Body FRD / world NED, SI physics, quaternion propagation, finite motor response and 50 Hz held PWM. All plant coefficients and analog endpoints are explicitly synthetic in results.json. No collision fidelity, battery sag, prop aerodynamics or RF calibration is claimed.",
        "Native library uses real Imu/Pilot/Estimator/Receiver/Safety/Stabilizer/Pid/mix/pwmDuty. Wire/ADC/time/network/virtual PWM are HALs; no real output API is linked. Idealized return status availability is not an ESP-NOW callback/scheduler simulation.",
        "", "## Filter experiments", "", "| Case | tau s | Roll RMS deg | Pitch RMS deg | Max pitch error deg | Yaw drift deg/min |", "|---|---:|---:|---:|---:|---:|"]
    for r in s["filters"]:
        lines.append(f"| {r['name']} | {r['tau_s']} | {r['roll']['rmse_deg']:.4f} | {r['pitch']['rmse_deg']:.4f} | {r['pitch']['max_abs_deg']:.4f} | {r['yaw_drift_deg_min']:.3f} |")
    lines += ["", "MAE, maximum error, settling, response delay and invalid-sample recovery are also in results.json; null means no observed settling/crossing or not applicable. Retain firmware tau=.5 s: compare .25/1.0 s across all traces, without selecting a single favorable graph. Neither coefficient fixes acceleration/tilt ambiguity or yaw observability.","", "![Filter noise comparison](plots/filter_noise.png)","", "![Translational acceleration](plots/filter_translation.png)","", "## Controller and full-chain scenarios", "", "| Scenario | Roll RMS deg | Pitch RMS deg | Rise s | Overshoot % | Settle s | Steady error deg | Final altitude m | State |", "|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
    fmt=lambda v:"unresolved" if v is None else f"{v:.3f}"
    for r in s["scenarios"]:
        lines.append(f"| {r['scenario']} | {r['roll']['rmse_deg']:.3f} | {r['pitch']['rmse_deg']:.3f} | {fmt(r['rise_time_s'])} | {fmt(r['overshoot_pct'])} | {fmt(r['settling_s'])} | {r['steady_state_error_deg']:.3f} | {r['altitude_final_m']:.3f} | {r['final_state']} |")
    lines += ["", "Boot/calibration/arming use a simulated support fixture until 9.5 s at 2 m. It is then released; this is not autonomous takeoff. roll_step/pitch_step/combined_step/saturation are component tests with encoded demands and an ideal gravity-only tilt sensor. All glove scenarios use accelerometer specific force including translation. RMS uses samples after 10 s. No altitude hold exists. Host execution microseconds are not ESP32 timing.","", "## Mixer / virtual ESC", "", "| Case | M1 FR | M2 FL | M3 RL | M4 RR | PWM us M1..M4 |", "|---|---:|---:|---:|---:|---|"]
    for r in s["mixer"]:
        lines.append("| "+r["name"]+" | "+" | ".join(f"{x:.3f}" for x in r["motors"])+" | "+", ".join(f"{x:.2f}" for x in r["pwm_us"])+" |")
    lines += ["", "These torque signs derive from documented FR/FL/RL/RR positions and reported CCW/CW spins. Physical motor layout remains unverified. Command, quantized PWM, simulated RPM and T=kT*omega^2 are separate trace fields. No real thrust curve is implied.","", "## Network and ADC", "", "```json",json.dumps(s["network"],indent=2),"```","", "![ADC fixtures](plots/analog.png)", "", "## Parameter sensitivity", "", "| Parameter | Multiplier | Steady error deg | Overshoot % | State |", "|---|---:|---:|---:|---:|"]
    for r in s["sensitivity"]: lines.append(f"| {r['parameter']} | {r['multiplier']} | {r['steady_error_deg']:.3f} | {fmt(r['overshoot_pct'])} | {r['final_state']} |")
    lines += ["", "## Acceptance checks and unresolved findings", "", "| Check | Result | Evidence / limitation |", "|---|---|---|"]
    for r in s["checks"]:lines.append(f"| {r['name']} | {r['status']} | {r['detail']} |")
    if "comparison" in s:lines += ["", "## Before and after", "", "```json",json.dumps(s["comparison"],indent=2),"```"]
    lines += ["", "## Reuse, changes, and remaining validation", "", "Pilot and Stabilizer orchestration were extracted into shared headers and used by the ESP32 adapters and native bridge. PID gains and motor-test behavior remain preserved. The SIL cycle identified a pitch-envelope validity defect; the final change and regression are documented in CHANGELOG.md. Before/after source fingerprints distinguish the tested states.",
        "Real MPU calibration, temperature/vibration, ADC wiring/fault conditioning, flash persistence, ESC startup/shutdown, measured thrust/inertia/lag/sag, RF and RTOS latency, power integrity and airborne failsafe remain unvalidated. Link or sensor fault cuts virtual output, so a falling simulated vehicle is expected; this does not establish a safe airborne policy.",
        "Full configuration, metrics and bounded traces are in results.json and trace_*.json alongside this report. Rerun tests and the report after changing any model or firmware. Seeded runs reproduce numerical traces; wall-clock timings and environment metadata are intentionally machine dependent.",""]
    path.write_text("\n".join(lines),encoding="utf-8")
