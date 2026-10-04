# Bison External Interface Contract

Status: **Frozen for Bison V1 architecture**

## Purpose

Bison is a reusable DUT helper with two distinct external interface domains:

- **Infrastructure side** — connections to the host/test environment.
- **DUT side** — connection to the DUT-specific fixture/interposer system.

The detailed electrical allocation of DUT-facing signals will be derived later from the Renesas RA6M3 peripheral and pin budget. The external connector topology itself is frozen here.

## Infrastructure side

Bison's infrastructure interfaces are Ethernet, service USB-C and a rear IEC operating-power inlet.

### Ethernet

Ethernet is an infrastructure interface, not a DUT-facing protocol interface.

Its intended roles include:

- remote/networked control;
- high-throughput data transport;
- deployment inside servers and test racks;
- coordination of multiple Bison units;
- PTP-based time synchronization.

Ethernet is the **supported external host interface** for Bison.

It is the network-native path for installed test infrastructure, external integrations, remote operation, and high-throughput data transport.

### USB-C

USB-C is retained as an **internal/service interface** for APSL bring-up, development, recovery, and direct bench work.

USB is not part of the supported external/customer-facing host interface contract. External integrations should use Ethernet.

This avoids making Bison's external product identity or distribution model depend on USB VID/PID allocation. Informally: the project owner strongly dislikes the USB-IF VID/PID bureaucracy.

For internal/service use, Bison exposes one composite USB device with two CDC ACM interfaces.

During initial development:

**CDC0 — CLI**
- all bidirectional host/Bison communication;
- commands and responses;
- configuration;
- measurements;
- protocol payloads;
- structured host-to-device and device-to-host data.

**CDC1 — LOG**
- one-way human-readable output;
- printf-style firmware diagnostics;
- warnings and errors;
- state transitions;
- live bring-up traces.

During device bring-up and initial firmware-stability sprints, real-time human-readable logging is considered more valuable than a high-throughput DAQ stream.

Once reliable on-board persistent logging exists:

- CDC0 remains the CLI/control plane.
- CDC1 becomes a machine-readable high-throughput data plane for DAQ, captures, protocol traces, and bulk telemetry.
- firmware diagnostic logs move to on-board storage and are retrieved through the CLI when required.

### Bison operating power

A rear IEC inlet feeds an off-the-shelf isolated AC/DC module. The main board receives 12 V. Bison operating power is independent of DUT power, and remains on while DUT power is removed or cycled.

The barrel jack is **DUT POWER IN**, not Bison's housekeeping supply.

## DUT side

Bison exposes one or more female DB25 fixture ports plus separate DUT power connectors:

- **DUT POWER IN:** front barrel jack, polarity marked, supplying the external pass-through voltage.
- **DUT POWER OUT:** front 2-position 5.08 mm pluggable screw-terminal system with orange/black visual treatment and explicit + / − markings.
- **DUT / fixture:** female DB25 ports carrying fixed-function DUT-facing capabilities and the dedicated fixture interlock.

The exact DB25 port count is intentionally not frozen yet. Four ports are the current upper-bound packaging estimate used for enclosure sizing.

The Bison contract stops at the DB25. The customer may use IDC ribbon, discrete wiring, direct PCB mating, or another suitable fixture harness downstream.

The agreed voltage variants remain 3.3–10 V, 10–36 V and 36–60 V, with the same current rating across variants; the exact few-amp rating remains open.

## Fixture architecture

The expected DUT fixture uses DUT-specific interposer boards, typically:

- a top interposer;
- a bottom interposer;
- pogo pins contacting DUT pads or controls.

The interposers are primarily passive and own:

- DUT-specific geometry;
- pogo-pin placement;
- routing between Bison channels and DUT contacts;
- mechanical alignment and fixture features.

## Ribbon branching

The ribbon is intentionally designed to be torn into branches.

A typical fixture may divide it between top and bottom interposers, but:

- the split does not need to be 50/50;
- exactly two branches are not required;
- fixture designers may split and route conductors as required by DUT geometry.

Bison defines the electrical function of each conductor position. The fixture defines its physical destination.

## Conductor discipline

The preferred recurring ribbon pattern is:

```text
GND - SIG - SIG - GND
```

This provides regular return paths and works naturally for paired signals.

Functional pairs that need to remain together electrically should remain on the same ribbon branch.

## Connector family preference

The preferred Bison DUT connector family should support both:

1. ribbon-cable mating; and
2. direct board-to-board mating.

This permits both conventional split-ribbon fixtures and compact direct-mating interposers.

Connector family, pitch, and pin count remain open until the RA6M3 resource allocation is understood.

## Local control and indication

The front panel has one RGB illuminated momentary antivandal switch. Button actions and color meanings are already defined in [front-panel UI](front-panel-ui.md); they are not reopened here. Ethernet link/activity indication is handled according to the selected connector/PHY design.

## Cooling provisions

Reserve rear mounting space for two optional 40 mm fans and two front intake slots, above and below the main board. Fans use internal 12 V, one shared PWM control and two separate tach lines.

## Current architecture views

See [architecture preparation](architecture-preparation.md) for the functional-block contract, protection, power sequencing and implementation handoff. Connector physical placement remains subject to panel/mechanical detail except where already explicitly settled.

## Intentionally deferred

The following remain open until peripheral and pin budgeting:

- DB25 port count and final pin allocation;
- exact DB25 connector MPN and mounting details;
- exact DUT-facing signal allocation;
- number of protocol, analog, digital, contact-emulation, and timing-capable channels;
- exact fixed electrical standard for each protocol/interface;
- final physical implementation of the already-defined front-panel RGB button.

The external interface should not be reopened merely to consume available MCU pins. Internal capability should fit the frozen external product shape unless a concrete requirement shows that the interface is inadequate.

