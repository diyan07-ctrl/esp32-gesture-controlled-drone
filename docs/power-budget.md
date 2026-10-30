# Power budget and electrical calculations

Verified by arithmetic from user inventory: nominal pack 11.1 V; capacity 2200 mAh =2.2 Ah; nominal energy E=11.1×2.2=**24.42 Wh**. Standard 3S LiPo full charge is 3×4.2=**12.6 V**; verify chemistry and charger. Actual usable energy depends on load, age and temperature.

Continuous battery capability I=2.2×C A. **C is unknown**. Hypothetical 25C →55 A, 35C →77 A; neither is a claim about this pack. Obtain the actual continuous label rating, not burst rating. A 60 A example needs ≥27.3C with no margin; 25% design allowance gives 75 A or ≥34.1C. Label ratings alone do not prove acceptable voltage sag.

| Item | Assumption | Result | Evidence status |
|---|---|---|---|
| Hover motor current | 4×5–8 A | 20–32 A | Scenario only; no thrust/current data |
| High-power scenario | 4×15 A | 60 A | Sizing example, not motor specification |
| Drone electronics | 5 V×0.5 A | 2.5 W | Provisional allowance; measure RF peaks |
| Buck input at 11.1 V, 85% | 2.5/(11.1×0.85) | 0.265 A | Assumed efficiency/load |
| Proposed regulator rating | 5 V×2 A | 10 W output | Procurement envelope, not measured draw |
| Worst full regulator input at 9 V, 85% | 10/(9×0.85) | 1.31 A | Branch example; verify transient response |

Pack budget is 4×I_motor + I_buck, not 4×ESC label. At 15 A each and 0.265 A buck: 60.265 A; C≥27.4 before margin. Propulsion limits must be checked at full charge and worst sustained load, then compared with ESC thermal ratings and actual battery sag. Approximate sag ΔV=I×R_pack+I×R_path. For a hypothetical 40 mΩ pack at 60 A, sag alone is 2.4 V and pack internal dissipation 144 W. This is not a measured resistance.

Wire/connector continuous ratings must exceed actual derated currents. See [electrical design](electrical-design.md) for length-specific resistance examples. A connector's nominal marketing current is not an aircraft thermal test.

Flight time methodology: t_min=60×capacity_Ah×usable_fraction / average_current_A. At a hypothetical usable fraction 0.8, 20.265 A gives 5.21 min; 32.265 A gives 3.27 min. This **3.3–5.2 min scenario is not a prediction**; the aircraft may not hover at either current. Frame/motor/ESC/battery/wiring masses are unknown. Weigh the aircraft and obtain thrust/current curves to determine if 3S 2200 mAh is suitable at all. Do not infer feasibility from stored energy alone.
