# Robu.in and FlyRobo sourcing check

Checked 2026-10-30 through supplier pages and indexed supplier extracts. This is a dated snapshot, not a reservation or checkout confirmation. Some live requests returned 403/inaccessible or only partial content; stock observations are the displayed supplier state, not independently verified warehouse inventory. Indexed category results can lag product prices (for example the SimonK combo category showed ₹799 while the product extract showed ₹789). Shipping and postcode serviceability have not been checked.

|Supplier|Item|SKU|Displayed INR incl GST|Observed availability|Engineering disposition|Source|
|---|---|---|---|---|---|---|
|FlyRobo|A2212 1000 KV motor|81|428|In Stock|Existing hardware comparison only; not identified owned motor|[Supplier page](https://www.flyrobo.in/a2212_1000kv_brushless_motor_for_rc_airplane)|
|FlyRobo|A2212 1000 KV + SimonK 30 A combo|206|789|In Stock|Replacement comparison only; signal timing/levels still need manual|[Supplier page](https://www.flyrobo.in/1000kv-a2212-brushless-motor-with-simonk-30a-esc-for-rc-airplane-quadcopter-multirotor)|
|FlyRobo|ESP32 WROOM 32 38-pin Micro USB|979|428|In Stock|Existing comparison; exact board schematic required|[Supplier page](https://www.flyrobo.in/esp-wroom-32-nodemcu-module)|
|FlyRobo|MPU6050 breakout|17|218|In Stock|Existing comparison; check supply and pullups|[Supplier page](https://www.flyrobo.in/mpu_6050_6dof_3_axis_gyro_with_accelerometer_sensor_module/)|
|FlyRobo|Multirotor PDB|1392|129|In Stock|Candidate M02; copper current/thermal rating not independently verified|[Supplier page](https://www.flyrobo.in/esc-multirotor-power-distribution-battery-board-for-quadcopter-multi-axis-model)|
|FlyRobo|ReadyToSky 5 V 5 A UBEC|3583|353|In Stock|HOLD: description 2-6S vs table 2-8S; output table 5.25V +/-0.5V; board tolerance unresolved|[Supplier page](https://www.flyrobo.in/readytosky-5v-5a-ubec)|
|FlyRobo|Bonka 3S 2200 mAh 35C|2536|2298|In Stock|Replacement only; 77 A label arithmetic; loaded performance unknown; connector text inconsistent|[Supplier page](https://www.flyrobo.in/11.1v-2200mah-35c-3s1p-bonka-lipo-battery-bonka-high-capacity-lipo-battery)|
|FlyRobo|Zop 3S 2200 mAh 30C|192|1727|Out Of Stock|URL says 35C but body says 30C; not an owned-pack identification|[Supplier page](https://www.flyrobo.in/2200mah-11.1v-35c-3s-zop-power-lipo-battery)|
|FlyRobo|XT60 male/female pair|91|27|In Stock|HOLD: listing conflicts 30A vs 80A continuous; do not approve 60A main path|[Supplier page](https://www.flyrobo.in/xt60_male_female_bullet_connector_plug)|
|FlyRobo|12 AWG red silicone wire 1 m|3197|136|In Stock|Candidate; also need return wire and measured thermal sizing|[Supplier page](https://www.flyrobo.in/12awg-high-quality-red-silicon-wire-1meter)|
|FlyRobo|WH148 10k potentiometer|1666|24|In Stock|Candidate 2 units; secure controls; yaw is manual-center not spring-centered|[Supplier page](https://www.flyrobo.in/wh148-10k-ohm-3pin-15mm-shaft-single-potentiometer)|
|Robu.in|100 A multirotor PDB|28954|120|Catalog Add to Cart; direct stock unconfirmed|Candidate only; direct page inaccessible|[Supplier page](https://robu.in/product-tag/power-distribution-board/)|
|Robu.in|5V/6V 3A switch-mode BEC||345|Catalog listing; stock unconfirmed|Identify exact model and 5V jumper setting before selection|[Supplier page](https://robu.in/product-category/drone-accessories/)|
|Robu.in|47k 1% 0603 resistor|573853|0.35|In Stock|SMD candidate upper divider only; lower resistor/protection still needed|[Supplier page](https://robu.in/product/47k-ohm-1-4w-0603-surface-mount-chip-resistor-pack-of-100/)|
|FlyRobo|1045 10x4.5 propeller||Unknown|Unconfirmed; page later returned 403|NOT selected; exact motor-specific prop load verification required|[Supplier page](https://www.flyrobo.in/1045_propeller_10in_10x4.5_for_drone)|

## Missing-hardware cost subset

For budgeting only, a provisional subset is PDB ₹129 + two WH148 controls ₹48 = **₹177**. This covers only 2 of the missing mandatory BOM categories and does not finalize the PDB rating or control mechanics. Buck, complete connector/wire set, protection circuit, switches, 2.5 V reference, glove supply, remaining passives, assembly supplies, props and test access remain unpriced or unapproved. Existing components and possible replacement batteries are excluded. The full remaining build cost is unknown.

No dedicated flight controller is selected, including any bundled controller advertised on supplier pages. Existing motors, ESCs, boards and battery do not need replacement merely because comparison listings were found.

## Explicit unresolved sourcing

No verified purchase candidate yet for: motor-specific CW/CCW props, 2.5 V regulated reference, normally closed run-permit switch, glove arm/enable switches, complete 1% divider pair and protection, all decoupling/filter parts, properly rated genuine main connector, return/branch wire quantities, protected glove power supply, mechanical retention and calibration equipment. Availability of a generic component family does not close these specification gates.

The UBEC output tolerance conflict must be resolved before applying it to a DevKit 5 V rail. Generic XT60 marketing current conflicts block approval for a 60 A example path. Supplier motor claims are not manufacturer validation of the owned motors. A 35C replacement battery label yields 2.2×35=77 A nominal claimed capability; it says nothing about the existing pack's C-rating or measured sag.
