# Bison Engineering Log

## 2026-10-03 — Host UI, onboarding and recipe architecture documented

The host-side product model is now recorded before recipe-language design begins.

The normal OOBE is Ethernet-first: power Bison, connect Ethernet directly or through a qualified USB-to-Ethernet accessory, and open `bison.local`. Bison should attempt DHCP and fall back to IPv4 link-local, advertising mDNS in either case. Manual networking belongs in troubleshooting, not ordinary onboarding. Service USB remains service-only pending a customer-facing VID/PID.

The host UI is split into a lean device-resident appliance UI and a heavier Internet-hosted engineering UI. Bison itself must provide Live, current/run logs, measurements, faults, controls, existing recipe selection/execution and basic device management offline. Recipe creation/editing, richer validation, Git workflows and asset management may be downloaded from the APSL-hosted web application and communicate locally with Bison from the browser.

Recipes are defined as plain-text, Git-versioned project artifacts and represent the complete executable DUT test definition: Bison configuration, signal mapping, sequencing, cause/effect expectations, timing, pass/fail rules, flash/templates, parameters, captures and operator steps. Git is the source of truth; Bison may cache recipes and must snapshot the exact recipe/assets used for each run.

Ten canonical tests were selected to drive the DSL/editor design: basic power-up, firmware flash + boot, cause/effect timing, analog transfer function, fault injection/recovery, protocol transaction, power sequencing, endurance/repeat, operator-assisted flow and parameterized production test. Syntax remains intentionally open until these are expressed in a minimal pseudo-language.

See [host UI and recipe architecture](host-ui-and-recipes.md).

## 2026-10-03 — Architecture preparation walkthrough documented

The external interfaces and functional blocks have been synthesized. This is a preparation-phase documentation checkpoint, not an implementation start.

Agreed refinements: independent IEC/isolated AC/DC/12 V operating power; DUT barrel-input pass-through variants 3.3–10 V, 10–36 V and 36–60 V with a common few-amp current target; orange/black DUT output screw terminals; dedicated pre-power check learned in the new-DUT wizard; soft-start followed by voltage checks; optional DUT-side differential ADC sensing; hardware clamp-current shutdown; complementary bleed switching with dead time; 470 µF initial discharge assumption and space for a few 2012 imperial resistors; optional dual 40 mm rear fans, front intake slots, shared PWM and separate tach inputs; I2C thermal monitors with wired-OR alerts.

Reversed DUT power raises DUT ground positive relative to Bison, so full positive fault exposure must be checked against the voltage variant. The provisional source-cutoff budget is 10 µs sensing plus 10 µs FET turn-off. Neither slow ramp nor resistance precheck guarantees detection, and stored energy/peak current still require validation.

Carry forward settled protocols, RGB-button behavior, modular power board, fixed logic levels, fixture interlock and proprietary product direction. Channel count and RA6M3 resource allocation remain implementation-phase work.

See [architecture preparation record](architecture-preparation.md) for decisions, block responsibilities, assumptions and handoff checks.

## 2026-10-02 — Digital I/O protection reshapes the product architecture

### Context

Bison started from a relatively simple assumption: the unit would act as a reusable DUT helper / functional-test controller, with DUT power passed through a replaceable power stage and Bison's own housekeeping power derived from the same general power arrangement.

The interface-protection discussion materially changed that picture.

As soon as the DUT-facing I/O was treated as reusable lab equipment rather than dev-board GPIO, several requirements emerged together:

- no RA6M3 pin should be exposed directly to the DUT;
- push-pull DUT I/O should use deterministic dual-supply translation;
- output contention must be treated as an overcurrent / thermal fault;
- fast hardware current detection should force the affected translator domain Hi-Z;
- positive external overvoltage needs a dedicated clamp strategy;
- ESD must be treated as a system-level connector problem;
- powered/unpowered DUT and partial-contact cases must remain safe;
- protection must apply to high-speed interfaces as well as slow GPIO.

This moved Bison conceptually toward PLC-style protected I/O, but for 3.3 V / 5 V embedded electronics rather than 24 V industrial field wiring.

That is a much stronger product foundation than a conventional MCU board with level shifters.

---

### Translator and protection domains

The current preferred translation primitive is a 74x245-class dual-supply, direction-controlled translator with explicit OE / Hi-Z and partial-power-down behavior.

Protection should not be fixed as "one monitor per pin" or "one monitor per package."

Instead, protection-domain width is a tuning knob.

Possible widths include:

- 1 signal;
- 2 signals;
- 4 signals;
- 8 signals;
- multiple translator packages sharing one monitored supply domain.

The objective is to use the widest domain that still leaves clear current margin between legitimate operation and a damaging fault.

An 8-channel protection domain is currently a sensible cost-oriented starting point, with finer granularity only where the current budget requires it.

A fast current-sense/comparator device such as an INA301-class part is a credible protection mechanism at this granularity.

The protection path is intended to be hardware-controlled:

```
contention / short
       |
       v
translator-domain current rises
       |
       v
fast current comparator
       |
       v
latched fault
       |
       v
translator OE disabled
       |
       v
affected domain Hi-Z
```

The translator only needs to survive the fault for the detector + latch + OE-disable delay, not indefinitely.

---

### Positive overvoltage protection

Output contention and externally applied overvoltage are different fault mechanisms.

A signal driven above the translator's safe output-pin voltage may not create a useful VCCB-source current signature, so positive overvoltage needs its own protection path.

The current concept is:

```
DUT ---- Rsmall ----+---- translator pin
                    |
                    +---- clamp diode ----> VCLAMP_POS
```

The small series resistor is not intended to be the primary current limiter for output contention. Its purpose is to make the clamp network practical while preserving high-speed signal integrity.

The positive clamp rail is hardware-defined and sits above the maximum legitimate DUT HIGH level but below the translator's unsafe region.

Clamp-current monitoring can provide a bank-level overvoltage indication without adding a voltage monitor to every pin.

The positive clamp rail should not simply dump sustained fault current into VCCB unless VCCB is explicitly designed to sink it.

---

### Negative excursions

For the primary Bison use case, negative excursions are expected to come from ringing, ground bounce, cable/fixture inductance, or partial contact rather than from legitimate bipolar signals.

The current V1 direction is therefore simple:

```
DUT ---- Rsmall ----+---- translator pin
                    |
                    +---- Schottky clamp ----> GND
```

The intended envelope is roughly enough to survive bad digital undershoot on the order of approximately -0.7 V to -1 V.

A dedicated small negative clamp rail remains a valid future architecture, especially for analog/bipolar-capable Bison variants, but it would require substantial calculation and validation around regulation, sink capability, startup, stability, thermal behavior, and fault response.

The idea is intentionally preserved but deferred.

---

### ESD

System-level ESD is treated separately from HBM/CDM component ratings.

Externally exposed DUT-facing signals should use low-capacitance IEC 61000-4-2 protection at the connector boundary, with short, low-inductance discharge paths.

The current provisional target is Level 4 / approximately +/-8 kV contact discharge, subject to later compliance planning.

The protection stack is therefore becoming:

- ESD: fast system-level transient protection at the connector;
- positive overvoltage: small series resistor + positive clamp rail;
- negative excursion: small series resistor + ground-referenced clamp;
- contention / short: monitored translator-domain current + hardware OE shutdown.

---

### VCCB architecture

The earlier idea of generating a separate VCCB rail per bank is unnecessarily expensive if Bison only needs a small number of standard logic levels.

The cleaner architecture is to generate shared global logic rails, initially:

- 3.3 V I/O;
- 5 V I/O.

Each translator domain selects the required rail.

This reduces duplicated DC/DC circuitry and makes the number of independent voltage domains mainly a routing/protection question rather than a converter-count problem.

The current order-of-magnitude expectation is a small number of voltage/protection domains, likely around four or fewer unless a real use case proves otherwise.

This is not a frozen channel-count requirement.

---

### VCLAMP architecture

Once VCCB is generated from shared global rails, the positive clamp rails can also be shared by voltage profile rather than generated independently for every bank.

The clamp rail must behave as a current sink, not merely as a voltage source.

Two implementation paths remain valid:

#### Shunt-regulator clamp rail

A TLV431/TL431/LT1431-class programmable shunt can establish VCLAMP and absorb injected clamp current.

Advantages:

- simple;
- inexpensive;
- naturally sinks current;
- easy hardware-set threshold.

Open questions:

- sink-current requirement;
- power dissipation;
- dynamic impedance;
- transient response;
- stability;
- feed-current sizing.

#### Op-amp + MOSFET active sink

An op-amp/error amplifier can control a MOSFET so that the clamp bus is held at the chosen threshold.

Advantages:

- scalable current capability;
- flexible threshold;
- independent choice of control and power devices.

Open questions:

- loop stability;
- phase margin with clamp-bus capacitance;
- startup/shutdown behavior;
- MOSFET SOA;
- fault response;
- failure mode.

The implementation choice is intentionally deferred until schematic design. The architecture only requires a hardware-defined clamp rail capable of sinking the expected aggregate clamp current while keeping translator pins within their safe voltage envelope.

---

### Major power-architecture consequence

The clamp discussion exposed an earlier bad assumption.

If the DUT uses 5 V logic and Bison needs a clamp rail above 5 V, Bison cannot reliably generate all of its protection and logic infrastructure from DUT power alone.

Therefore Bison power and DUT power must be separate.

The revised invariant is:

> Bison and DUT power may arrive or disappear in any order. Hardware must always converge to DUT power OFF and DUT I/O Hi-Z unless Bison is fully powered and deliberately enables them.

Corollary:

> No externally powered DUT path may back-power Bison or defeat the default-OFF / default-Hi-Z state.

This applies to:

- DUT voltage/current sensing;
- power-stage control;
- translator pins;
- clamp networks;
- EEPROM/I2C crossing points;
- fault/status paths;
- any gate-driver/control circuitry crossing the two power domains.

---

### Power-entry ergonomics

Bison power and DUT power are electrically separate, but the external connection does not necessarily need to look like two casual hot-plug connectors.

A screw-terminal style DUT-power interface remains attractive because it communicates installation wiring rather than casual hot swapping.

However, connector insertion order cannot be treated as a safety mechanism.

The hardware must remain safe regardless of which external source appears first.

---

### Grounding consequence

Once Bison and DUT power are separate, common-ground topology becomes an architectural issue.

The system should distinguish:

- Bison power return;
- DUT power return;
- protective earth / chassis where applicable;
- the intentional signal-reference relationship between Bison and the DUT.

The intention is to control where Bison and DUT references meet rather than allowing two arbitrary external supplies to determine that relationship.

This initially led to considering externally isolated supplies or an onboard isolated DC/DC stage.

---

### Isolation and mains power

A premium isolated desktop supply with a proprietary/locking connector was investigated, but it is economically unattractive if the PSU costs a significant fraction of or more than Bison itself.

An onboard isolated DC/DC is technically possible, but at an estimated total Bison input power around 20 W it becomes a meaningful BOM, thermal, and EMI block.

That led to a cleaner possibility:

> Treat Bison as actual test equipment and bring mains into the enclosure.

A likely product architecture is now:

```
IEC mains inlet
  + switch
  + fuse
  + EMI filter
        |
        +---- PE --------> chassis
        |
        +---- L/N -------> isolated AC/DC supply
                              |
                           BISON DC RAIL
                              |
                         Bison electronics
```

The AC/DC conversion may be:

- board-mounted and encapsulated; or
- chassis-mounted.

At the current stage, a chassis-mount AC/DC supply is especially attractive because it can keep mains conversion physically away from the logic board and leave the main PCB largely SELV.

A combined IEC inlet with integrated fuse, switch, and EMI filter is also a natural fit.

Mains handling itself is not considered a blocker for the project.

---

### Enclosure is now part of the architecture

The enclosure can no longer be deferred as cosmetic packaging.

A metal benchtop / rack-style enclosure may simplify:

- mains entry;
- PE/chassis bonding;
- AC/DC mounting;
- Ethernet connector placement;
- DUT-power screw terminal placement;
- wide DUT/fixture connector placement;
- ESD discharge paths;
- cooling;
- physical separation of mains and low-voltage electronics;
- chassis/reference-ground strategy.

This means enclosure and internal mechanical architecture should be considered before committing to the final power implementation.

---

### Product-level insight

The protection work changed how Bison should be thought about.

It is no longer merely:

> a microcontroller board that helps test another board.

It is converging toward:

> a robust, networked, protected low-voltage I/O and DUT-power platform for embedded-system test and automation.

That makes the hardware architecture itself potentially core commercial IP.

The combination of:

- protected configurable low-voltage I/O;
- deterministic Hi-Z behavior;
- hardware contention shutdown;
- overvoltage and ESD protection;
- protocol interfaces;
- programmable/measured DUT power;
- fixture interlock;
- Ethernet control;
- test automation;

can form the foundation for fixture controllers, production-test systems, HIL systems, lab automation, and other APSL commercial products.

As a consequence, the earlier idea of open-sourcing Bison hardware is no longer assumed. Bison should currently be treated as proprietary product architecture unless there is a specific reason to expose part of it later.

---

### Open decisions

The following remain intentionally open:

- exact translator family and package widths;
- exact protection-domain widths;
- exact overcurrent threshold;
- exact current-sense/comparator device;
- exact positive clamp voltages;
- exact clamp diode and series-resistor values;
- shunt regulator vs op-amp/MOSFET VCLAMP sink;
- exact number of voltage/protection domains;
- exact global VCCB rails beyond 3.3 V / 5 V;
- exact AC/DC topology and power-supply part;
- board-mounted vs chassis-mounted AC/DC;
- enclosure style and dimensions;
- exact chassis / Bison 0 V / DUT 0 V bonding strategy.

The current goal is architectural feasibility and fault-model completeness, not premature circuit selection.

