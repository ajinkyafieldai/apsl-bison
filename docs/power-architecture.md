# Bison Power Architecture

Status: **Architecture frozen; power-module implementation intentionally deferred**

## Purpose

Bison uses a split power architecture consisting of:

- a **common control board**; and
- a **replaceable power board**.

The goal is to keep the control platform stable while allowing different DUT voltage/current classes, measurement ranges, and future programmable-supply capabilities to be implemented on separate power boards.

This document defines the architectural boundary between those boards.

It does **not** define the detailed power-supply implementation.

## Why the power section is modular

Bison is expected to support DUTs across a broad family envelope, potentially from low-voltage embedded boards through 48 V-class systems.

Trying to implement one universal 3.3 V-to-48 V power path would significantly complicate:

- DUT power switching;
- range-specific DUT power circuitry;
- voltage measurement accuracy;
- current measurement accuracy;
- current-shunt sizing;
- amplifier range and common-mode requirements;
- transient protection;
- thermal design;
- calibration.

The difficulty is especially acute if Bison is expected to provide useful voltage and current readings over that entire range.

The selected architecture therefore keeps the control electronics common and moves the range-sensitive power and measurement functions to a replaceable power board.

## Board split

### Control board

The control board contains the common Bison infrastructure and intelligence, including:

- Renesas RA6M3;
- USB;
- Ethernet;
- DUT-facing protocol and control interfaces;
- host communications;
- test orchestration;
- firmware;
- the DUT ribbon interface;
- the control-side interface to the power board.

The control board should not need a different firmware image for each power-board variant.

### Power board

The power board owns the DUT power path and its range-specific circuitry.

Its responsibilities include:

- barrel-jack power input;
- DUT pre-power stimulus/measurement and output discharge;
- DUT power switching;
- DUT voltage sensing;
- DUT current sensing;
- protection appropriate to the supported voltage/current class;
- local power fault handling;
- module identity;
- calibration data.

The first power board may simply provide switched pass-through of the barrel-jack voltage to the DUT.

Future power boards may implement a more capable programmable supply.

## External power model

The barrel jack provides the intended DUT supply voltage.

For the initial pass-through power module, the external barrel jack feeds DUT switching, protection and sensing, then the separately labeled DUT POWER OUT screw terminals. Bison does not regulate the DUT supply voltage.

Bison housekeeping power is independent: rear IEC inlet → off-the-shelf isolated AC/DC module → 12 V input to the main board → internal rails. The mains module is outside the main board. The earlier shared barrel-jack housekeeping model is superseded.

## Product-family voltage range

The agreed power-board/product voltage variants are:

| DUT input range | Current rating |
|---|---|
| 3.3–10 V | Same across variants |
| 10–36 V | Same across variants |
| 36–60 V | Same across variants |

The current target is a few amps; exact rating and range-boundary tolerances remain open. No single universal board is required.

DUT POWER IN is a barrel jack with clear polarity marking. DUT POWER OUT uses a 2-position 5.08 mm pluggable screw-terminal system with orange/black visual treatment and explicit + / − markings.

Each variant needs suitable switching, sensing, transient protection and thermal margins.

## Precheck, soft-start, discharge and faults

The architecture includes a dedicated low-current pre-power measurement path, controlled main-path ramp, hardware overcurrent cutoff, output voltage/current telemetry, and switched bleed resistors. Main-switch and bleed-switch control is complementary with dead time.

The new-DUT wizard records precheck response/limits and optional DUT-side voltage sensing through differential DB25 analog channels.

The precheck now includes explicit reverse-polarity validation before full-power enable. Exact implementation remains open, but a detected swapped DUT power connection must block enable.

The precheck is an additional preventive layer, not the Bison-survival guarantee. Hardware protection must still tolerate reversed wiring if the check is bypassed, defeated, or inconclusive; DUT survival is not guaranteed.

Clamp, logic-supply and thermal faults feed the hardware shutdown interlock. The controller does not perform time-critical protection.

Use 470 µF as the initial discharge sizing assumption; at 60 V this stores about 0.85 J. Reserve space for a few 2012 imperial resistors, with actual values and pulse ratings selected during implementation.

The full decisions and provisional 20 µs shutdown budget are recorded in [architecture preparation](architecture-preparation.md).

## Measurement and calibration

Accurate voltage and current measurement is one of the main reasons to split the power section from the control board.

Each power board should carry its own calibration data.

A small EEPROM on the power board is the preferred identity and calibration mechanism.

The EEPROM may contain:

- module type;
- hardware revision;
- serial number;
- supported voltage range;
- supported current range;
- voltage calibration coefficients;
- current calibration coefficients;
- shunt/gain metadata;
- absolute operating limits;
- calibration metadata as needed later.

The control board reads this information at startup and applies the appropriate scaling and limits.

A resistor-ID matrix is not preferred once per-module calibration is introduced.

## Control interface

The control board should command the power board through a simple fixed interface.

The preferred command signal is **PWM**.

### PWM role

For a simple pass-through power module:

- 0% duty cycle means DUT power OFF.
- 100% duty cycle means DUT power ON.

The power board may treat this as a simple enable signal.

The PWM interface is intentionally chosen so that future power modules can interpret intermediate duty cycles as a commanded output level.

This preserves the control-board interface while enabling more capable power modules later.

The exact PWM frequency and electrical details remain to be defined during implementation.

### Power-board status and telemetry

The board-to-board interface should provide at least:

- PWM / power command from control board to power board;
- power fault/status from power board to control board;
- voltage-sense signal;
- current-sense signal;
- I2C for power-board EEPROM;
- required logic supply and ground connections;
- required raw/high-current power paths.

The exact connector and pinout are deferred until the internal electrical design is worked out.

## Future programmable power supply

The modular boundary is intentionally designed so that a future power board can become a true programmable DUT supply without redesigning the Bison control board.

A future module may add capabilities such as:

- adjustable DUT voltage;
- programmable current limit;
- controlled power-up/down ramps;
- programmable undervoltage;
- brownout injection;
- step drops;
- slow supply sag;
- recovery ramps;
- repeated brownout cycles;
- improved telemetry;
- additional fault injection.

For such a module, the PWM command can represent a requested output level rather than merely ON/OFF.

This gives Bison a path to controlled brownout and power-fault testing while preserving the same control-board architecture.

## Safety boundary

Safety-critical protection should remain local to the power board.

The control board may command requested power behavior, but the power board should enforce its own hard electrical limits and fault protection.

Safety-critical current or voltage limits should not depend solely on PWM interpretation or host-side software.

## Intentionally deferred

The following are explicitly **not being designed yet**:

- tolerances and margins around the agreed power-board voltage ranges;
- exact current ranges;
- switch topology;
- pass FET selection;
- current-shunt value;
- current-sense amplifier;
- voltage-divider network;
- protection circuitry;
- downstream internal-rail converter topology;
- programmable-supply topology;
- PWM frequency and transfer function;
- power-board connector;
- thermal design;
- detailed calibration process.

These decisions should be made only after the rest of the Bison interface and RA6M3 resource allocation are sufficiently defined.

The current goal is to freeze the **architectural boundary**, not prematurely design the power supply.

