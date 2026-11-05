# Ground station implementation design

Status: specification for a separate application, not an installed dashboard.
Target Python 3.11 on Windows initially. Use PySide6, pyqtgraph, NumPy, PyOpenGL
and pyserial. [pyqtgraph](https://www.pyqtgraph.org/) supports Qt plots and 3D
graphics; use its OpenGL view for a procedural F450 mesh without introducing a
second rendering engine. Pin versions after installation and launch verification;
record Qt/Python/OpenGL versions and dependency licenses before packaging.

## UI and ownership

Dark charcoal surfaces, restrained cyan measurements, amber degraded health and
red fault indications; pair color with text. Put serial/FC/link/arming state in a
persistent top strip. The central resizable view contains an F450 X frame with
450 mm diagonal motor spacing, center plates, four motor cans and four props;
front arms have distinct color. Mechanical details are illustrative until frame
dimensions are measured. Propeller rotation must not imply measured RPM: keep
props static unless actual RPM sensing is added.

Provide ground grid, axes, orbit/zoom and reset camera. Apply the documented FRD
body-to-NED quaternion through one scene transform. Test identity, each positive
axis and wraparound; physical tests must confirm the displayed tilt direction.
Interpolate only measured attitudes, with normalized shortest-path SLERP. On
stale/offline data freeze the last pose, dim it and show its age; startup shows a
static identity model explicitly labeled OFFLINE. Never generate fake live data.

Four motor bars show actual commanded PWM and normalized command, requested
collective/correction where available, output inhibition and motor location.
No tachometer, thrust or spinning status is inferred from PWM. Large angle/rate
cards label yaw RELATIVE / DRIFTING. Separate throttle command, pot percent,
allocated collective and output inhibition. Tabbed lower panels hold PID
setpoint/measurement/error/P/I/D/total plots, raw/scaled IMU, calibration, RF
accepted/lost/rejected/rate/age, USB gaps, loop timing/max/misses and faults.
Battery panel defaults NOT INSTALLED; percent remains unavailable without a
documented calibrated model.

## Threads and bounded storage

The serial worker owns the port with bounded read timeout, COBS/CRC parser,
record-length cap, boot/sequence tracking and receipt timestamp. Partial frames
remain buffered up to a fixed maximum; oversize input discards through the next
delimiter. Reject wrong versions/types/lengths/nonfinite/out-of-range data without
changing displayed state. Stop/disconnect must unblock promptly and close the
port before reconnect. Throttle worker-to-GUI notification and keep one latest
snapshot so queued signals cannot grow without bound.

The GUI samples that snapshot at a bounded render rate (target 60 Hz); plots
update more slowly (target 20 Hz) over fixed-duration ring buffers. A distinct
bounded logging queue writes versioned binary/raw records plus receive times,
metadata and gaps. Log overflow is reported, never blocks the serial worker.
Replay uses recorded timestamps and the same decoder, visibly labeled REPLAY;
serial acquisition is disconnected during replay. CSV export preserves units,
session IDs, validity and missing data. Malformed sessions produce an error,
not an invented continuation.

## Controls and configuration

Implement port scan/selection, baud, connect/disconnect, telemetry display
start/stop, fullscreen, plot pause/resume/reset, recording, session replay/speed,
CSV export, camera reset and parameter monitoring. Plot pause does not pause
acquisition or conceal connection alarms. Display start/stop controls desktop
consumption; any firmware streaming request must be noncritical and bounded.

PID editing is a later, separately tested configuration-mode capability. It needs
an onboard local permit, disarmed/low-throttle guards and validated transactional
ACK/readback; persistence needs explicit save confirmation. Initial desktop build
is read-only and must not claim parameter editing is implemented. No live motor
commands or arming controls are exposed.

## Acceptance and installation plan

After requirements are pinned, installation will use a dedicated environment:

~~~powershell
py -3.11 -m venv .venv-ground
.\.venv-ground\Scripts\python.exe -m pip install -r ground_station/requirements.txt
.\.venv-ground\Scripts\python.exe ground_station/main.py
~~~

These are future interface commands; the application and requirements file do
not exist yet. Do not present them as runnable in the current README. Identify
the actual board USB bridge before recommending CP210x/CH340 drivers. Port
selection must use actual enumeration; historical COM9 is not an assignment.

Acceptance requires codec golden vectors shared with C++, corruption/partial/
oversize/resync/wrap/reboot tests; serial disconnect/reconnect and bounded-buffer
tests; quaternion transform/SLERP tests; log roundtrip and replay isolation; a
real Windows OpenGL launch with screenshots; offline/stale visual inspection;
actual two-board RF-to-USB display tests; measured rate, drops, CPU, memory and
receipt-to-render latency under load. A headless import does not prove rendering.
No benchmark or hardware display result has been recorded for this application.
