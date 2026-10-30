# Battery analysis and voltage sensing

[Power budget](power-budget.md) contains energy, C-rating and endurance calculations. Before use, record pack maker, chemistry, continuous C, connector, age, swelling/damage, per-cell balance and loaded voltage. Use a compatible balance charger and its instructions in a suitable nonflammable location. Do not charge damaged packs.

## Divider

Proposed upper R1=47 kΩ, lower R2=10 kΩ, both 1%, midpoint to ADC1 GPIO34 through 1 kΩ series. Place 100 nF across R2 at the quiet electronics ground. Vadc=Vpack×R2/(R1+R2)=12.6×10/57=**2.2105 V**. Divider current=12.6/57000=**0.2211 mA**. R1 power≈2.30 mW; R2≈0.489 mW. 0.125 W resistors easily cover nominal dissipation, but voltage/transient ratings still apply. Worst high reading at tolerances is 12.6×10.1/(46.53+10.1)=**2.247 V**.

The divider Thevenin resistance is 8.246 kΩ; 100 nF yields τ≈0.825 ms, fc≈193 Hz. ADC series resistor and sampling capacitance add dynamics. The filter is for noise, not an input protection guarantee. Check ADC settling and average measurements in hardware. Software uses calibrated Arduino millivolt conversion at 11 dB attenuation (classic ESP32), ratio 5.7 and adjustable scale. Stay in characterized ADC range; 3.3 V is not a target measurement voltage or a universal clamp threshold. See [Espressif ADC documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/peripherals/adc.html). ADC nonlinearity, reference variation, resistor tolerances and board noise require DMM calibration at several pack voltages.

With motors disconnected, apply a current-limited known source, compare DMM pack voltage with telemetry, calculate scale=V_DMM/V_reported and recheck across the range. Disconnect/supply faults yielding <6 V or >13.2 V are flagged as implausible. A stuck-but-plausible ADC reading is not detectable by this implementation. Protection against transients/back-power when MCU is unpowered needs review before connection; do not leave a live divider connected to an unpowered board without verified input protection.

## Low voltage

Provisional bench warning/arm-inhibit is 10.8 V; critical is 9.9 V. These are not finalized pack thresholds. Current firmware conservatively inhibits/latches outputs when voltage is invalid or below its acceptable level while armed; it has no automatic landing capability. Raw single-sample low-voltage handling can false-trigger under sag; filtering, persistence and cell-level monitoring require characterization before flight. Never use motor cutoff as normal airborne battery management. A warning must allow the pilot to land with reserve after a future validated flight policy exists.
