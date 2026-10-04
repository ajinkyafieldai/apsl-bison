# Bison Mechanical Architecture

## Purpose

This document captures the current mechanical architecture and enclosure decisions for Bison.

The intent is to establish the physical product constraints early enough that enclosure geometry, PCB outline, panel design, PSU mounting, connector placement, and assembly flow are developed coherently.

---

## 1. Product form factor

Bison V1 is a **true 19-inch 1U rack instrument**.

The production mechanical baseline is:

- standard 19-inch rack width / 482.6 mm front-panel format;
- 1U nominal height / 44.45 mm rack envelope;
- short-depth chassis, with roughly 250-300 mm as the current planning range;
- direct rack-ear mounting rather than a separate tray or half-width carrier;
- optional removable feet for bench use.

The earlier compact desktop-extrusion concept is superseded.

### 1.1 Rack constraint

The hard mechanical target is:

> Bison shall fit a standard 19-inch 1U server/test rack as a native rack-mount instrument.

The chassis, front panel, connector layout, airflow and service access should therefore be designed directly around rack installation rather than treating rack use as an accessory mode.

---

## 2. Enclosure construction

The preferred construction is a low-cost commodity **1U rack-mount sheet-metal chassis** suitable for instrumentation and test equipment.

Desired characteristics:

- standard 19-inch rack ears/front-panel geometry;
- approximately 44 mm chassis height;
- short-depth construction where practical;
- removable top cover;
- separately machinable or replaceable front/rear panels;
- straightforward chassis mounting for the isolated AC/DC module;
- practical internal standoffs or rails for the main PCB;
- black powder-coated or similar durable finish where available;
- suitable for local custom cutouts, legends and printing.

The exact supplier and chassis depth remain open. Indian commodity rack-chassis vendors are preferred where they meet the mechanical and electrical requirements.

### 2.1 Superseded enclosure concepts

The earlier Yongu-style low-profile extrusion, Virtus extrusion fallback, slide-in PCB guide concept, and optional rack-tray strategy are superseded by the native 19-inch 1U chassis decision.

They may remain useful historical references but must not constrain the production mechanical design.

---

## 3. Main PCB mechanical retention

The native rack chassis removes the earlier requirement to slide the production PCB in extrusion guide rails.

The main PCB may use conventional chassis standoffs, a removable internal tray, or another serviceable mounting scheme selected with the final commodity 1U chassis.

The production mounting scheme should:

- support the PCB independently of front-panel connectors;
- avoid excessive connector loading from fixture cable insertion/removal;
- permit practical assembly and service;
- preserve intentional chassis/PE bonding;
- leave the rear AC/DC module mechanically independent.

The existing four PCB holes remain useful for bench-development standoffs, but their production role is no longer constrained by the old extrusion concept.

---

## 4. Front panel architecture

The front panel is the rack chassis front panel, not a PCB.

It is machined/punched for the Bison connectors and controls and carries durable printed or silk-screened legends.

The front panel carries:

- six DUT/fixture DB25 connectors as three stacked pairs;
- infrastructure Ethernet;
- DUT POWER IN;
- DUT POWER OUT;
- operator pushbutton;
- READY / ACTIVE / FAULT indicators;
- recessed RESET access.

Front-panel connectors may be PCB-mounted or chassis-mounted as appropriate, but mechanical insertion loads must be carried by the chassis/panel rather than relying on PCB solder joints alone.

The previous front-panel PCB/card-edge/gold-finger concept is superseded.

---

## 5. Rear panel architecture

The rear panel is aluminum.

It is expected to carry higher-energy / infrastructure connections, particularly mains entry.

Current intended rear-panel functions include:

- IEC mains inlet;
- integrated or adjacent fuse;
- mains switch;
- EMI filter where practical;
- any rear DUT power wiring/connector placement, subject to the current external contract;
- PE/chassis bond point;
- any other low-frequency/service connections better suited to the rear.

The exact rear-panel connector set remains open.

---

## 6. AC/DC power-supply mounting

The current power direction is to bring mains into the enclosure and use an isolated AC/DC supply for Bison housekeeping power.

A chassis-mount AC/DC supply is a strong candidate.

The AC/DC supply must be:

- mounted directly to the enclosure/chassis;
- mechanically independent of the main PCB;
- installed using the manufacturer-specified number and location of mounting points / standoffs;
- positioned with appropriate mains clearance and airflow.

The main PCB is not a structural carrier for the AC/DC supply.

---

## 7. Grounding / chassis implications

The mechanical design must preserve the distinction between:

- protective earth / chassis;
- Bison low-voltage 0 V;
- DUT power return;
- intentional Bison-DUT signal-reference bond.

The enclosure provides a deliberate metal chassis for:

- PE bonding;
- ESD discharge;
- connector-shield bonding;
- EMC control.

Any PCB-to-chassis connection must be intentional.

Mounting hardware should not accidentally create uncontrolled signal-ground/chassis bonds.

---

## 8. Assembly sequence

The exact production assembly sequence depends on the selected 1U commodity chassis, but the intended order is now conventional rack-instrument assembly:

1. prepare the chassis base and front/rear panels;
2. install PE/chassis bond hardware and mains-entry hardware;
3. install the isolated AC/DC module independently to the chassis;
4. install the main PCB and any internal interface/power-board assemblies;
5. install front-panel and rear-panel connectors/controls;
6. complete low-voltage and mains wiring with required segregation;
7. perform open-chassis inspection and electrical checks;
8. install the top cover;
9. perform final functional/safety inspection.

The important principles are serviceability, mechanical support of heavily used connectors, accessible internal wiring before closure, and independent mounting of the mains AC/DC module.

---

## 9. Rack integration

Rack mounting is the primary mechanical mode.

Bison is a native 19-inch 1U instrument with integral rack ears/front-panel mounting.

Bench use remains supported by optional removable feet; it is not a separate enclosure architecture.

---

## 10. Open mechanical decisions

Still to be determined:

- exact 1U chassis supplier / part number;
- final chassis depth within the short-depth target;
- exact AC/DC supply and mounting pattern;
- exact IEC inlet / switch / fuse / filter arrangement;
- exact front/rear panel thickness and manufacturing process;
- detailed placement and spacing of the three stacked DB25 pairs and other front-panel interfaces;
- detailed cooling validation, vent dimensions and exact fan placement;
- final main-PCB / power-board mounting method;
- final chassis / PE / signal-ground bond details.

---

## 11. Current mechanical direction

Current decisions/directions:

- use a **native 19-inch 1U rack chassis**;
- target a short depth of roughly 250-300 mm unless internal layout proves otherwise;
- use integral rack ears/front-panel mounting;
- support bench use with removable feet;
- use six front DB25s arranged as three stacked pairs;
- use a machined/punched and printed rack front panel;
- do not use a front-panel PCB or card-edge interconnect;
- mount the chassis AC/DC supply independently using its specified mounting points;
- use conventional serviceable PCB/chassis mounting appropriate to the selected rack chassis;
- keep IEC mains, service USB-C and cooling on the rear;
- preserve front Ethernet, DUT power connectors, operator control/status and fixture connectors.

## 12. Preparation update on 3 October 2026

The operating supply is an off-the-shelf isolated AC/DC module providing 12 V to the main board. Mains remains outside the main-board electrical boundary.

Reserve space for two optional rear 40 mm fans and two front intake slots, above and below the main board. Fan power is 12 V directly, with one shared PWM command and separate tach inputs. I2C thermal monitoring feeds the hardware fault line.

DUT POWER IN is a polarity-marked barrel jack. DUT POWER OUT uses a 2-position 5.08 mm pluggable screw-terminal system with orange/black visual treatment and explicit polarity legends. These connector types supersede earlier provisional terminal descriptions; panel placement still follows the final mechanical layout.

Cooling provisions must be validated experimentally against the selected enclosure and 1U target. One or two 40 mm or 20 mm square fans may be added if testing shows they are required; do not freeze fan count or size from packaging sketches alone. See [architecture preparation](architecture-preparation.md).

