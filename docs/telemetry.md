# Telemetry and debugging

Drone emits D-prefixed CSV snapshots at at most 5 Hz. Formatting is in the low-priority Arduino task and uses a fixed buffer; writes occur only when TX buffer capacity suffices. Otherwise a snapshot is dropped. A latest-snapshot queue bounds memory. Serial TX buffer requests 1024 bytes before begin; verify allocation and hardware behavior. Logging rate is cfg::logHz. Set a positive lower rate for quieter diagnostics; disabling logs needs an explicit guard before setting zero.

Fields after D: ms,state,faults,battery_V,age_ms,exec_us,misses,rejected,lost,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,roll_deg,pitch_deg,yaw_deg,throttle,roll_cmd,pitch_cmd,yaw_cmd,pid_r,pid_p,pid_y,m1,m2,m3,m4. Commands are normalized throttle, angle degrees and yaw degrees/s. Motor values are normalized requests, never measured speed. age_ms=4294967295 means no accepted packet. exec_us is the snapshot-time loop duration; use external timing to characterize the entire period and jitter.

Glove G fields: ms,sensor_healthy,status_fresh,command_permitted,throttle,ack_age_ms. Low-rate serial messages can be dropped. Status packets additionally expose ack sequence, state, faults and battery. Gap statistics include mailbox overwrites and cannot isolate RF loss. Transport sendErrors/dropped counters exist but are not currently exported over the status packet; inspect with a debug build or add a versioned extension.

Capture with a serial terminal at 115200 baud into a local CSV/log file, then run:
```sh
python tools/telemetry_parser.py capture.csv > named.csv
```
The parser rejects truncated D records. Check state transitions and fault bits first, then sensor axis signs, age, missed deadlines and saturation. Correlate power/ESC edge traces with logs. A successful serial print does not prove control-loop deadlines or motor behavior.
