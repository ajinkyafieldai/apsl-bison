# Bison V1 DUT-Facing Interface Set

Status: **Frozen at high-level functional/electrical contract**

## Purpose

This document freezes the initial Bison V1 DUT-facing interface set before detailed Renesas RA6M3 peripheral allocation and schematic design.

The goal is not to define exact channel counts yet. It is to define which interface classes Bison should support and the intended electrical behavior of each class.

Bison V1 uses fixed-function interfaces rather than runtime-configurable universal channels.

## Design principles

- Prefer fixed, explicit electrical interfaces over configurable or universal translation schemes.
- Prefer dedicated level translators appropriate to each protocol class.
- Keep DUT-specific interposers primarily passive.
- Use Bison as a board-level DUT helper rather than trying to reproduce every external physical-layer standard.
- Probe logic-level nodes on the DUT where practical instead of duplicating external transceivers unnecessarily.
- Final channel counts will be determined during RA6M3 peripheral and pin budgeting.

## UART

Bison should provide fixed board-level UART interfaces at:

- 3.3 V logic;
- 5 V logic.

RS-232 and RS-485 are not baseline requirements for Bison V1.

Where a DUT exposes RS-232 or RS-485 externally, the preferred fixture strategy is to probe the logic-level UART side of the DUT transceiver using pogo pins when practical.

This keeps Bison focused on board-level test access rather than duplicating every external line-driver standard.

## I2C

Bison should provide:

- one 3.3 V I2C interface class;
- one 5 V-tolerant I2C interface class.

The DUT is expected to provide the bus pull-ups.

Bison participates as an open-drain bus device and should not inject unnecessary DUT-side pull-ups onto an existing bus.

Dedicated bidirectional/open-drain level translation should be used rather than a universal translator.

## SPI

Bison should provide fixed SPI interface classes at:

- 3.3 V;
- 5 V.

Bison should be designed **slave/peripheral-first**, because the expected common use case is for Bison to emulate a peripheral while the DUT acts as SPI controller/master.

From Bison's perspective:

- SCK: DUT -> Bison
- MOSI: DUT -> Bison
- MISO: Bison -> DUT
- CS: DUT -> Bison

Dedicated direction-aware level translation should be used.

The need for master-capable SPI channels can be revisited later if a concrete test case requires them.

## CAN

Bison should provide fixed isolated CAN.

The intended characteristics are:

- isolated CAN transceiver;
- software-configurable bus termination;
- CAN_H and CAN_L exposed to the DUT-side fixture;
- isolation and termination implemented on Bison rather than on the DUT interposer.

The exact transceiver part remains to be selected.

## Contact emulation

Bison should provide floating two-terminal contact-emulation channels intended to replace physical switches or buttons.

Each channel exposes two DUT-side pins and should behave electrically as an isolated normally-open contact.

PhotoMOS is preferred if practical because it allows firmware to synthesize deterministic switch behavior, including:

- ideal clean presses;
- programmable make/break patterns;
- controlled bounce sequences;
- precise press/hold/release timing.

An electromechanical relay remains acceptable if board area, voltage/current range, leakage, on-resistance, or other electrical constraints make it a better choice.

## Generic digital inputs

Generic digital inputs are fixed-direction observation channels.

All generic digital inputs should be **5 V tolerant**.

They should be conditioned/protected before reaching the RA6M3.

These channels are intended for DUT state observation such as:

- READY/BUSY;
- FAULT;
- interrupt lines;
- status GPIO;
- boot/status outputs;
- other non-protocol digital signals.

## Generic digital outputs

Generic digital outputs are fixed-direction stimulus channels.

Bison should provide fixed outputs at:

- 3.3 V logic;
- 5 V logic.

No runtime voltage selection is required.

These channels are intended for DUT stimulus such as:

- enable lines;
- boot straps;
- control GPIO;
- interrupt injection;
- watchdog stimulus;
- other non-protocol digital controls.

## Digital input/output ratio

The target generic digital channel ratio is approximately:

```text
1 input : 2 outputs
```

Exact channel counts will be selected during RA6M3 pin/resource budgeting.

## Analog voltage inputs

All Bison analog measurement channels should share the same basic external contract:

```text
0-5 V DUT input -> active analog front end -> 0-3.3 V RA6M3 ADC range
```

The analog front end should use an amplifier/buffered scaling stage rather than relying only on a passive divider.

This provides a consistent measurement interface and gives control over:

- input impedance;
- ADC drive impedance;
- filtering;
- protection;
- scaling accuracy.

A mix of single-ended and differential measurements is required. The DUT wizard records measurement mode, assigned input/reference or pair, and acceptance limits. Optional DUT-side rail sensing uses these differential ribbon channels, not a new external connector.

The exact op-amp, bandwidth, input impedance, overvoltage margin, filtering and differential common-mode envelope remain implementation details. Differential mode does not imply direct 60 V input tolerance.

## Level-translation philosophy

Bison V1 should use **dedicated translators per interface class**.

The project intentionally does not use a universal programmable level-shifting architecture for UART, SPI, I2C, or generic GPIO.

Rationale:

- better electrical determinism;
- easier validation;
- better reliability across different DUTs;
- fewer protocol-specific corner cases;
- clearer documentation and fixture expectations.

Push-pull interfaces should use direction-aware translators.

Open-drain interfaces such as I2C should use translators designed specifically for bidirectional/open-drain signaling.

## Power interface

The DUT power subsystem is defined separately in `docs/power-architecture.md`.

The key architectural points are:

- separate common control board and replaceable power board;
- barrel jack provides the DUT supply for the initial pass-through module;
- programmable DUT power switching;
- voltage/current telemetry;
- power-board EEPROM for identity and calibration;
- PWM control from the control board;
- future path to programmable voltage and brownout/fault-injection testing.

Detailed power-supply design is intentionally deferred.

## Host / infrastructure interface

The infrastructure-side interface is defined separately in `docs/external-interface.md`.

Current V1 direction:

- Ethernet is the supported external host interface;
- USB-C is retained as an internal/service interface;
- wide ribbon cable is the DUT-side fixture interface.

## Deferred decisions

The following remain open:

- exact number of UART channels;
- exact number of I2C channels;
- exact number of SPI channels;
- exact number of CAN channels;
- exact number of contact-emulation channels;
- exact generic digital input/output counts;
- exact analog input count;
- exact translator and protection components;
- exact voltage/current limits for contact channels;
- exact analog front-end implementation;
- exact RA6M3 pin/peripheral mapping.

These should be resolved by going deeper into each peripheral class and then performing the RA6M3 resource budget.

