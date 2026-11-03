# Serial telemetry

115200 baud. D snapshots pass through a bounded latest-value queue, with formatting in Arduino loop rather than the flight task. Log writes are dropped if TX space is insufficient. D fields: ms,state,faults,battery_V,age_ms,exec_us,misses,rejected,lost,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,roll_deg,pitch_deg,yaw_deg,throttle,roll_cmd,pitch_cmd,yaw_cmd,pid_r,pid_p,pid_y,m1,m2,m3,m4. State/fault codes are in [safety](safety-and-failsafe.md). Motors are requests, not RPM. Battery zero means monitoring unavailable. Age 4294967295 means no accepted packet. Execution snapshot is not an external worst-case timing measurement.

G fields: ms,imu_healthy,flex_healthy,status_fresh,gesture_enum,flex1_raw,flex2_raw,throttle. Glove boot also reports IMU/radio/calibration status and station MAC when available. It never logs keys. U fields, only when ultrasonic acquisition is enabled: ms,driver_ready,range_healthy,filtered_meters,invalid_count. Range becomes unhealthy after 300 ms without an accepted echo; retained distance must not be mistaken for a fresh measurement.

python tools/telemetry_parser.py capture.csv converts complete D lines; G/U remain diagnostic text. Keep captures local, identify firmware commit/configuration and correlate with instrument traces. Transport counters are internal; status exports reject counts but not all RF metrics.
