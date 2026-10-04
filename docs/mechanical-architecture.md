# Bison Mechanical Architecture

## Purpose

This document captures the current mechanical architecture and enclosure decisions for Bison.

The intent is to establish the physical product constraints early enough that enclosure geometry, PCB outline, panel design, PSU mounting, connector placement, and assembly flow are developed coherently.

---

## 1. Product form factor

Bison is intended to work in two physical modes:

- benchtop instrument;
- optional 1U rack-mounted instrument.

The enclosure itself does not need to occupy a full 19-inch rack width.

The preferred model is:

- compact standalone enclosure;
- removable rubber feet for benchtop use;
- optional 1U rack bracket / tray;
- feet removed when the unit is installed in the rack bracket.

### 1.1 Height constraint

The hard mechanical target is:

> Bison enclosure body should fit within a 1U rack-height envelope.

1U is 44.45 mm nominal.

The preferred enclosure family should therefore target approximately 30-35 mm body height where practical, leaving margin for manufacturing tolerance and rack hardware.

The 1U requirement is a production target. A taller enclosure may be tolerated for an early revision if sourcing blocks progress.

---

## 2. Enclosure construction

The preferred construction is a low-cost commodity extruded aluminum electronics enclosure rather than a custom folded-sheet-metal chassis.

Desired enclosure characteristics:

- aluminum extrusion / U-shell construction;
- removable front and rear end panels;
- internal PCB guide slots;
- black anodized finish where available;
- inexpensive standard sizes;
- suitable for custom front/rear panel machining;
- compatible with an optional rack tray/bracket.

### 2.1 Reference family

Yongu-style low-profile extruded aluminum PCB enclosures are the current mechanical reference.

The preferred reference envelope is:

- approximately 30-35 mm high;
- width and depth chosen around final PCB and PSU requirements;
- removable end panels;
- PCB guide grooves;
- commodity construction.

Indian/local sourcing should be explored using the Yongu profile/dimensions as the reference.

Direct import from Yongu or equivalent Chinese suppliers is acceptable for prototypes and low-volume units.

### 2.2 Rev A fallback

Virtus Fab / similar Indian-stock extruded enclosures remain an acceptable Rev A fallback if low-profile sourcing becomes a schedule blocker.

The main limitation of the currently identified Virtus-style enclosure is height, not construction quality or suitability.

The design should continue against the Yongu-style low-profile constraints unless a concrete sourcing issue requires otherwise.

---

## 3. Main PCB mechanical retention

The main PCB is not intended to be screwed down at multiple mounting points in the production enclosure.

The extrusion and front/rear geometry provide the primary constraints.

### 3.1 Z constraint

The extrusion PCB guides constrain the main PCB vertically.

The board slides into the enclosure through the front opening.

### 3.2 Longitudinal / XY retention

A cylindrical spacer / hard stop is mounted to the chassis floor near the rear edge of the main PCB.

The spacer is not a threaded PCB mounting point.

Its purpose is simply:

> physically prevent the main PCB from sliding farther rearward.

The main PCB therefore slides in from the front until its rear edge contacts the stop.

The front-panel/card-edge assembly captures the front side of the board.

### 3.3 No snap-in support

A snap-in PCB support was considered and rejected because its chassis feature would protrude from the enclosure bottom and interfere with the desired flush-bottom / rack-tray arrangement.

The current direction is:

- countersunk chassis fastener;
- cylindrical spacer / stop inside the enclosure;
- no protruding snap feature under the chassis.

### 3.4 Bench-test holes

The main PCB will include four mounting holes.

These holes are:

> for bench testing and development standoffs only.

They are not intended to be used for normal production enclosure mounting.

This keeps bring-up convenient without forcing the product enclosure to depend on four blind standoff locations.

---

## 4. Front panel architecture

The front panel is a PCB.

The front-panel PCB provides:

- visible panel graphics / legends;
- front-facing connectors;
- LEDs / indicators;
- any front-panel controls;
- chassis/connector-shield bonding features where required.

### 4.1 Main-board interconnect

The preferred interconnect is:

> vertical card-edge connector on the front-panel PCB mating with gold fingers on the main PCB.

Conceptually:

```
front PCB
|  vertical card-edge connector
|          ||
|          ||
+----------||---- main PCB gold fingers
```

Benefits:

- no harness or FFC;
- clean assembly;
- repeatable alignment;
- low part count;
- easy front-panel replacement;
- main PCB can slide directly into the connector during final assembly.

### 4.2 Gold fingers

The main PCB should use proper card-edge geometry:

- beveled edge;
- appropriate hard-gold finish for repeated mating;
- connector keepout;
- multiple ground contacts;
- sufficient mating-depth tolerance.

The connector should not be treated as the sole structural support for the main PCB.

Mechanical loads are still carried by the enclosure guides, chassis stop, and front-panel attachment.

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

The current intended assembly sequence is:

1. Prepare the lower enclosure section.
2. Install the AC/DC power supply.
3. Install rear-panel hardware, including IEC mains hardware and DUT power terminal.
4. Install the aluminum rear panel as allowed by the chosen enclosure geometry.
5. Install the cylindrical rear stop/spacer for the main PCB.
6. Slide the main PCB into the extrusion guides from the front until the rear edge contacts the stop.
7. Wire the AC/DC supply, rear-panel mains hardware, DUT power path, and main PCB.
8. Perform open-chassis inspection / electrical checks.
9. Install / slide on the upper U-shell section.
10. Mate the front-panel PCB card-edge connector to the main-board gold fingers.
11. Screw the front panel to the enclosure.

The exact order of rear-panel and upper-shell installation may vary with the final Yongu-style enclosure geometry.

The important assembly principles are:

- mains hardware is installed before the visible front PCB;
- the main PCB slides in without requiring multiple blind standoff alignments;
- internal wiring remains accessible before enclosure closure;
- the front-panel PCB is installed last.

---

## 9. Rack integration

Rack mounting should be optional.

The preferred model is:

- Bison remains a standalone compact instrument;
- an optional 1U tray / bracket accepts the enclosure;
- rubber feet are removed for rack installation;
- the rack bracket carries rack-mounting loads rather than requiring the enclosure itself to be full-rack width.

The final enclosure width should preserve the possibility of efficient rack use, but no requirement currently exists to fit two Bison units side-by-side.

---

## 10. Open mechanical decisions

Still to be determined:

- exact Yongu enclosure family / part size;
- final width and depth;
- exact body height within the 1U target;
- exact AC/DC supply and mounting pattern;
- exact IEC inlet / switch / fuse / filter arrangement;
- rear-panel connector set;
- final front-panel DB25 count and connector layout;
- exact cylindrical stop geometry;
- exact front/rear panel thickness;
- optional rack bracket / tray design;
- detailed cooling validation, vent dimensions and exact fan placement;
- final chassis / PE / signal-ground bond details.

---

## 11. Current mechanical direction

Current decisions/directions:

- target a low-profile Yongu-style extruded aluminum enclosure;
- maintain a production target of 1U-compatible body height;
- allow a taller Virtus-style enclosure as a Rev A fallback if necessary;
- use removable rubber feet for benchtop use;
- use an optional 1U rack bracket/tray;
- use the enclosure's aluminum front plate, machined for the interfaces and silk-screened for legends/branding;
- do not use a front-panel PCB or card-edge interconnect;
- use an aluminum rear panel;
- constrain the main PCB in Z with extrusion guides;
- use a single rear cylindrical stop to prevent further rearward PCB travel;
- do not screw the production main PCB to the stop;
- provide four PCB holes only for bench-test standoffs;
- mount the chassis AC/DC supply independently using its specified mounting points;
- use countersunk chassis fasteners where a flush bottom is required;
- install the machined/silk-screened aluminum front plate as part of final enclosure assembly.

## 12. Preparation update on 3 October 2026

The operating supply is an off-the-shelf isolated AC/DC module providing 12 V to the main board. Mains remains outside the main-board electrical boundary.

Reserve space for two optional rear 40 mm fans and two front intake slots, above and below the main board. Fan power is 12 V directly, with one shared PWM command and separate tach inputs. I2C thermal monitoring feeds the hardware fault line.

DUT POWER IN is a polarity-marked barrel jack. DUT POWER OUT uses a 2-position 5.08 mm pluggable screw-terminal system with orange/black visual treatment and explicit polarity legends. These connector types supersede earlier provisional terminal descriptions; panel placement still follows the final mechanical layout.

Cooling provisions must be validated experimentally against the selected enclosure and 1U target. One or two 40 mm or 20 mm square fans may be added if testing shows they are required; do not freeze fan count or size from packaging sketches alone. See [architecture preparation](architecture-preparation.md).

