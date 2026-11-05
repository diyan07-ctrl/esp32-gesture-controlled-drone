"""Reproducible numerical experiments; thresholds are engineering expectations, not flight claims."""
import copy
import math
import numpy as np
from .engine import Simulation, SCENARIOS
from .models import quaternion, rotation, body_rates, ImuModel


def error_metrics(error):
    a = np.asarray(error, dtype=float)
    return {"mae_deg":float(np.mean(np.abs(a))), "rmse_deg":float(np.sqrt(np.mean(a*a))),
            "max_abs_deg":float(np.max(np.abs(a)))}


def settling(time, error, start=0, tolerance=1.0):
    time, error = np.asarray(time), np.abs(np.asarray(error))
    indices = np.where((time >= start) & (error > tolerance))[0]
    if not len(indices): return 0.0
    last = indices[-1]
    return float(time[last+1]-start) if last+1 < len(time) else None


def filter_experiment(library, config, name, tau):
    rng = np.random.default_rng(config["seed"])
    sensor_config = copy.deepcopy(config["sensor"])
    sensor_config["gyro_bias_dps"] = [.3,-.2,.5]
    sensor_config["gyro_drift_dps_per_s"] = [.01,-.008,.02] if name == "bias_drift" else [0,0,0]
    if name == "noise":
        sensor_config["accel_noise_g"] = .04; sensor_config["gyro_noise_dps"] = 1
    imu = ImuModel(sensor_config,config["seed"]+10)
    records, previous = [], np.zeros(3)
    gyro_angles = np.zeros(3)
    t = 0.0
    recovery_start = recovery = None
    with library.filter(tau) as filt:
        while t < 20:
            dt = .004*(1+rng.uniform(-.25,.25)) if name == "jitter" else .004
            angle = np.zeros(3)
            if name in ("gradual","noise","jitter"):
                angle = np.array([20*math.sin(.4*t),10*math.sin(.3*t),0])
            elif name == "sudden":
                angle = np.array([20*(1-math.cos(math.pi*np.clip((t-4)/.2,0,1)))/2,
                                  -10*(1-math.cos(math.pi*np.clip((t-10)/.2,0,1)))/2,0])
            rate = body_rates(angle,(angle-previous)/dt)
            previous = angle.copy()
            acceleration = [3,0,0] if name == "translation" and 4 <= t < 12 else [0,0,0]
            sample = imu.sample(quaternion(angle), np.radians(rate),acceleration,dt,t)
            fault = name == "invalid" and ((8 <= t < 8.02) or (9 <= t < 9.02) or (10 <= t < 10.02))
            if fault:
                sample[0] = np.nan if t < 9 else 8.0 if t < 10 else np.inf
                recovery_start = t
            ok, estimate = filt.update(sample,dt)
            if recovery_start is not None and ok and recovery is None:
                recovery = t-recovery_start
            ar = np.degrees(math.atan2(sample[1],sample[2])) if np.all(np.isfinite(sample)) else np.nan
            ap = np.degrees(math.atan2(-sample[0],math.hypot(sample[1],sample[2]))) if np.all(np.isfinite(sample)) else np.nan
            # Gyro-only Euler kinematics in the same FRD convention, for comparison only.
            r,p = np.radians(gyro_angles[:2]); gx,gy,gz=sample[3:]
            gyro_angles += dt*np.array([gx+math.sin(r)*math.tan(p)*gy+math.cos(r)*math.tan(p)*gz,
                math.cos(r)*gy-math.sin(r)*gz,(math.sin(r)*gy+math.cos(r)*gz)/math.cos(p)])
            records.append([t,*angle,*estimate,ar,ap,*gyro_angles,int(ok),*sample])
            t += dt
    rows = np.asarray(records)
    usable = (rows[:,0] >= 2) & (rows[:,12] == 1)
    result = {"name":name,"tau_s":tau,"roll":error_metrics(rows[usable,4]-rows[usable,1]),
              "pitch":error_metrics(rows[usable,5]-rows[usable,2]),
              "raw_roll_rmse_deg":float(np.sqrt(np.nanmean((rows[usable,7]-rows[usable,1])**2))),
              "raw_pitch_rmse_deg":float(np.sqrt(np.nanmean((rows[usable,8]-rows[usable,2])**2))),
              "yaw_drift_deg_min":float(np.polyfit(rows[usable,0],rows[usable,6]-rows[usable,3],1)[0]*60),
              "roll_drift_deg_min":float(np.polyfit(rows[usable,0],rows[usable,4]-rows[usable,1],1)[0]*60),
              "recovery_s":recovery,"rejected_samples":int(np.sum(rows[:,12]==0)),
              "settling_s":settling(rows[:,0],rows[:,4]-rows[:,1],4.2 if name == "sudden" else 0),
              "response_delay_s":None}
    if name == "sudden":
        ground=np.where(rows[:,1]>=10)[0]; estimate=np.where(rows[:,4]>=10)[0]
        if len(ground) and len(estimate):
            result["response_delay_s"]=float(rows[estimate[0],0]-rows[ground[0],0])
    return result,rows


def scenario_metrics(simulation, rows):
    t=np.array([r["t"] for r in rows]); post=np.where(t>=10)[0]
    if not len(post): post=np.arange(len(t))
    truth=np.array([r["truth"] for r in rows]); estimate=np.array([r["estimate"] for r in rows])
    target=np.array([r["setpoint"] for r in rows])
    error=truth-target
    step_axis=0 if simulation.scenario != "pitch_step" else 1
    step_target=float(target[-1,step_axis])
    rise=overshoot=None
    if abs(step_target) > .01:
        response=truth[post,step_axis]/step_target
        low=np.where(response>=.1)[0]; high=np.where(response>=.9)[0]
        if len(low) and len(high): rise=float(t[post[high[0]]]-t[post[low[0]]])
        overshoot=float(max(0,np.max(response)-1)*100)
    motor=np.array([r["motor"] for r in rows])
    return {"scenario":simulation.scenario,"duration_s":simulation.time,"steps":simulation.steps,
        "roll":error_metrics(error[post,0]),"pitch":error_metrics(error[post,1]),
        "estimation_roll":error_metrics(estimate[post,0]-truth[post,0]),
        "estimation_pitch":error_metrics(estimate[post,1]-truth[post,1]),
        "rise_time_s":rise,"overshoot_pct":overshoot,
        "settling_s":settling(t,error[:,step_axis],10,1),
        "steady_state_error_deg":float(np.mean(error[-min(25,len(t)):,step_axis])),
        "oscillation_peak_to_peak_deg":float(np.ptp(truth[-min(50,len(t)):,step_axis])),
        "recovery_after_disturbance_s":settling(t,error[:,1],12.16,1) if simulation.scenario=="disturbance" else None,
        "altitude_start_m":float(-rows[post[0]]["position"][2]),
        "altitude_final_m":float(-rows[-1]["position"][2]),
        "final_state":rows[-1]["state"],"final_faults":rows[-1]["faults"],
        "fault_time_s":simulation.fault_time,"max_motor":float(np.max(motor)),
        "min_motor":float(np.min(motor)),"max_total_thrust_N":float(max(sum(r["thrust"]) for r in rows)),
        "last_thrust_N":float(sum(rows[-1]["thrust"])),
        "saturation_s":simulation.saturation_time,"first_fault_age_ms":simulation.first_fault_age_ms,
        "native_execution_max_us":simulation.max_native_us,
        "native_execution_p99_us":float(np.percentile([r["host_native_us"] for r in rows],99)),
        "sent":simulation.link.sent,"delivered":simulation.link.delivered,"dropped":simulation.link.dropped,
        "unique_delivery_rate":(simulation.link.sent-simulation.link.dropped)/max(1,simulation.link.sent),
        "arrival_copies_per_sent":simulation.link.delivered/max(1,simulation.link.sent),
        "max_consecutive_dropped":simulation.link.max_consecutive,
        "latency_mean_ms":float(np.mean(simulation.link.latencies)*1000) if simulation.link.latencies else 0,
        "latency_max_ms":float(max(simulation.link.latencies)*1000) if simulation.link.latencies else 0,
        "finite":bool(np.all(np.isfinite(truth)) and np.all(np.isfinite(motor))),
        "trace_records":len(rows),"trace_limit":simulation.config["trace_limit"]}


def network_experiment(library,config):
    from .models import Link
    c=copy.deepcopy(config["link"])
    c.update(loss=.15,delay_s=.03,jitter_s=.04,duplicate=.2,corrupt=.1)
    network=Link(c,config["seed"])
    accepted=rejected=0
    with library.context(config["analog_fixture"],False) as context:
        # Establish the required zero-flags/zero-throttle session first.
        context.receive(library.encode([0,0,0,0],0),0)
        for step in range(1500):
            t=step*.004
            if step%5 == 0: network.send(library.encode([0,0,0,0],step//5+1),t)
            for arrival,packet in network.receive(t):
                if context.receive(packet,round(arrival*1000)): accepted+=1
                else: rejected+=1
    return {"sent":network.sent,"delivered":network.delivered,"dropped":network.dropped,
        "duplicates":network.duplicates,"corrupted":network.corrupted,"accepted":accepted,
        "rejected":rejected,"max_consecutive_dropped":network.max_consecutive,
        "unique_delivery_rate":(network.sent-network.dropped)/network.sent,
        "arrival_copies_per_sent":network.delivered/network.sent,
        "loss_pct":100*network.dropped/network.sent,
        "latency_mean_ms":float(np.mean(network.latencies)*1000),
        "latency_max_ms":float(max(network.latencies)*1000)}


def mixer_experiment(library):
    cases={"minimum":[0,0,0,0],"maximum":[1,0,0,0],"roll":[.5,.1,0,0],
        "pitch":[.5,0,.1,0],"yaw":[.5,0,0,.1],"combined":[.5,.05,.04,.03],
        "saturation":[.9,.8,.8,.8],"invalid":[float("nan"),0,0,0]}
    result=[]
    for name,inputs in cases.items():
        v=library.mix(inputs)
        result.append({"name":name,"input":[None if not math.isfinite(x) else x for x in inputs],
            "valid":bool(v[0]),"saturated":bool(v[1]),"motors":v[2:6],
            "pwm_us":[d*1e6/(50*65535) for d in v[6:10]]})
    return result


def analog_experiment(library,config):
    f=config["analog_fixture"]
    rows=[]
    with library.context(f,False) as context:
        for i in range(3000):
            t=i*.004
            v=0 if t<1 else min(1,(t-1)/4) if t<6 else .5 if t<8 else 0
            thumb=.76+.025*math.sin(t*40) if 4<=t<8 else 1 if 2<=t<4 else 0
            raw=[f["thumb_open"]+(f["thumb_bent"]-f["thumb_open"])*thumb,
                 f["index_open"],f["pot_low"]+(f["pot_high"]-f["pot_low"])*v]
            out=context.analog(raw)
            rows.append([t,*raw,*out])
    rows=np.asarray(rows)
    high=rows[(rows[:,0]>=6.2)&(rows[:,0]<7.8),9] # bent state thumb: t/raw3/health3/flex2/bent2/target/value
    return {"throttle_min":float(rows[:,12].min()),"throttle_max":float(rows[:,12].max()),
            "hysteresis_switches_on_noisy_hold":int(np.count_nonzero(np.diff(high))),
            "synthetic_endpoints":f},rows
