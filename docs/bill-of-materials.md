# Bill of materials

The engineering BOM below leaves final selected-part prices **unknown** because exact specifications are not closed. Dated Robu.in/FlyRobo candidate prices and stock appear in [supplier availability](supplier-availability.md) and hardware/supplier-availability.csv. A provisional missing-hardware subset is ₹177 (PDB plus two controls); full remaining cost and existing hardware cost are unknown. Candidate stock is not compatibility approval. Consumables and test access are separate from missing hardware. Do not purchase propulsion or power components before their selection gates close.

|ID|Category|Component|Quantity|Required/Optional|Specification|Purpose|Compatibility status|Estimated price INR|Source|
|---|---|---|---|---|---|---|---|---|---|
|E01|Existing|F450 frame|1|Required|Quad-X; PDB rating unknown|Structure|Unverified|Unknown|Owner inventory|
|E02|Existing|ESP32-WROOM-32 DevKit|2|Required|Exact PCB and regulator unknown|Glove and onboard flight compute|Conditional|Unknown|https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32/hw-reference/esp32/get-started-devkitc.html|
|E03|Existing|MPU6050 breakout|2|Required|3.3 V logic; breakout supply unknown|Six-axis sensing|Conditional|Unknown|https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet.pdf|
|E04|Existing|A2212 motors|4|Required|1000 KV owner-reported; maker unknown|Propulsion|Unverified|Unknown|Owner inventory and KV report|
|E05|Existing|ESC|4|Required|Nominal 30 A; model and BEC unknown|Three-phase motor drive|Unverified|Unknown|Owner inventory|
|E06|Existing|LiPo battery|1|Required|3S 2200 mAh; continuous C unknown|Flight energy|Unverified|Unknown|Owner inventory|
|M01|Missing mandatory|Matched CW and CCW propellers|4|Required|Exact motor-specific diameter/pitch/hub pending|Thrust|Selection blocked|Unknown|Motor manufacturer curve required|
|M02|Missing mandatory|PDB or distribution harness|1|Required unless frame PDB verified|12.6 V plus transients; actual total-current rating|High-current distribution|Selection pending|Unknown|Rated supplier datasheet required|
|M03|Missing mandatory|Buck regulator|1|Required for selected architecture|Proposed 5 V 2 A; documented 3S/transient input and thermal rating|Drone electronics supply|Selection pending|Unknown|Exact manufacturer datasheet required|
|M04|Missing mandatory|Main isolation and battery connector pair|1 set|Required|Match actual battery polarity and derated measured total current|Battery disconnect|Selection pending|Unknown|Exact connector datasheet required|
|M05|Missing mandatory|Protected glove USB power bank and cable|1|Required|5 V; provisional >=1 A continuous; no auto-off at actual load|Independent glove power|Selection pending|Unknown|Supplier documentation required|
|M06|Missing mandatory|Throttle and centered yaw potentiometers|2|Required|10 kOhm; 0-2.5 V wiper; secured controls|Safe explicit pilot inputs|Provisional|Unknown|Supplier specification required|
|M07|Missing mandatory|2.5 V reference supply|1|Required|Low-impedance regulated 2.5 V; supply two 10k pots; verify stability|Analog input endpoints|Selection pending|Unknown|Reference datasheet required|
|M08|Missing mandatory|Arm and hold-to-enable switches|2|Required|Dry contacts to glove ground; 3.3 V logic|Explicit glove permission|Provisional|Unknown|Supplier specification required|
|M09|Missing mandatory|Normally closed local run-permit switch|1|Required|Dry contact to drone GND; opening inhibits|Local emergency input|Provisional|Unknown|Supplier specification required|
|M10|Missing mandatory|Divider resistors|2|Required|47 kOhm and 10 kOhm; 1%; >=0.125 W|Pack voltage sensing|Provisional; protection review pending|Unknown|Exact resistor datasheet required|
|M11|Missing mandatory|Signal/ADC resistors|13|Required|4x220 Ohm ESC; 4x10k pulldown; 3x1k ADC series; 2x100k pot pulldown|Conditioning and safe signal state|Provisional|Unknown|Exact resistor datasheet required|
|M12|Missing mandatory|ADC filter capacitors|3|Required|100 nF; suitable dielectric; battery and two pilot ADC inputs|Noise filtering|Provisional|Unknown|Component datasheet required|
|M13|Missing mandatory|Regulator and sensor decoupling|1 set|Required if not already fitted|Selected device datasheet values and voltage ratings|Rail stability|Inspect existing boards first|Unknown|Selected regulator and breakout documentation|
|M14|Missing mandatory|Input protection for divider|1 circuit|Required before connection|3.3 V-safe; no back-power with MCU unpowered; design pending|ADC and power sequencing protection|Unresolved|Unknown|Engineering review required|
|C01|Consumables|Power cable|1 set|Required|Length/current dependent; AWG12 main and AWG16 branch examples only|Power paths|Thermal validation pending|Unknown|Wire manufacturer rating required|
|C02|Consumables|Motor-phase connectors or soldered insulated joints|12 joints|Required|Actual branch current/temperature rated; 3 per motor|Motor hookup|Selection pending|Unknown|Connector datasheet required|
|C03|Consumables|Low-current wiring and pullups|1 set|Required|3.3 V I2C; add pullups only after inspecting breakout|Signal interconnect|Provisional|Unknown|Breakout schematic required|
|C04|Consumables|Heatshrink solder crimp hardware strain relief|1 set|Required|Temperature and mechanical suitability|Reliable insulated assembly|Selection pending|Unknown|Assembly supplier|
|C05|Consumables|Standoffs fasteners battery straps electronics mounts|1 set|Required|Motor bolt length verified; insulation and prop clearance|Retention|Selection pending|Unknown|Frame and motor manufacturer|
|O01|Optional|Low-ESR distribution capacitor|1|Optional pending ESC recommendations|Candidate 470-1000 uF >=25 V; ripple/inrush verified|Transient suppression|Selection pending|Unknown|ESC manufacturer guidance required|
|O02|Optional|LEDs and series resistors|2 sets|Optional|LED plus 1kOhm resistor each|Local status|Provisional|Unknown|LED datasheet|
|O03|Optional|Buzzer and transistor driver|1 set|Optional|Regulated supply; GPIO-safe driver; firmware pending|Audible warning|Not implemented|Unknown|Buzzer datasheet required|
|O04|Optional|Current sensor|1|Optional|Range above real pack peak; isolated or suitable common mode; 3.3 V-safe output|Power characterization|Not implemented|Unknown|Sensor datasheet required|
|O05|Optional|IMU vibration mount|1|Optional after measurement|No uncharacterized soft resonance|Reduce vibration|Measure first|Unknown|Mechanical test|
|O06|Optional|Propeller guards|1 set|Optional site dependent|Correct prop clearance and mass accounting|Contact mitigation|Selection pending|Unknown|Frame/prop-specific supplier|
|T01|Test equipment|DMM and current-limited bench supply|1 each|Required test access|Adequate 3S voltage; verified polarity and rails|Electrical validation|Availability unknown|Unknown|Calibrated equipment|
|T02|Test equipment|Oscilloscope or logic analyzer|1|Required test access|Resolve 50 Hz frame and 1000-2000 us pulse plus jitter|ESC and I2C timing|Availability unknown|Unknown|Instrument manufacturer|
|T03|Test equipment|Balance charger and cell checker|1 each|Required test access|Correct 3S LiPo chemistry; pack instructions|Battery management|Availability unknown|Unknown|Charger manufacturer|
|T04|Test equipment|Guarded thrust rig current logger scale|1 set|Required before propulsion validation|Rated for selected prop and maximum thrust/current|Thrust power and mass measurements|Availability unknown|Unknown|Rig/instrument documentation|
