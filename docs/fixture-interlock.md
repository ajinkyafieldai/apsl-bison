# Bison Fixture Interlock and Hot-Swap Robustness

Status: **Core architectural requirement**

## Purpose

Bison is intended for bed-of-nails and similar DUT fixtures where the DUT is mechanically engaged through pogo pins.

A dedicated fixture interlock signal is required so Bison can enter a safe electrical state before pogo contacts disengage.

At the same time, Bison should be robust enough that violating the intended fixture sequence does not damage Bison itself.

The DUT is not guaranteed protection from misuse. Bison is.

## Core fixture signal

Bison provides a dedicated fixture signal:

```text
FIXTURE_INTERLOCK
```

This is a core fixture input, not a generic GPIO.

The preferred electrical behavior is fail-safe:

- asserted = fixture fully engaged / testing permitted;
- deasserted or open circuit = fixture not safe / testing prohibited.

A broken wire or disconnected interlock should therefore be treated as an open fixture.

## Mechanical contract

The fixture should be designed so that the interlock deasserts **before pogo contact begins to break** during fixture opening.

Expected opening sequence:

```text
fixture begins to open
        |
        v
FIXTURE_INTERLOCK deasserts
        |
        v
Bison enters safe state
        |
        v
pogo pins disengage
```

The fixture designer should provide enough mechanical travel between interlock release and pogo disengagement for Bison to quiesce the DUT interface.

This is consistent with common bed-of-nails / functional-test fixture practice where lid or clamp interlocks remove power or halt test activity before operator access or fixture disengagement.

## Bison safe-state behavior

Deassertion of `FIXTURE_INTERLOCK` should force Bison into the DUT-safe electrical state.

At minimum:

- DUT power OFF;
- all level-translator banks Hi-Z;
- generic driven outputs disabled;
- contact-emulation channels open;
- active protocol drivers disabled where practical.

DUT power-up should be prohibited unless the interlock is asserted.

## Bank-voltage interaction

The interlock state is part of Bison's bank-voltage state machine.

A bank VCC may only change while:

- DUT power is OFF; and
- the affected translators are Hi-Z.

Bison power-on should follow the general sequence:

```text
1. DUT power OFF
2. All DUT-facing translators Hi-Z
3. Configure bank VCCs
4. Enable bank supplies
5. Wait for bank rails to settle / verify
6. Enable translators
7. Enable DUT power
```

Fixture opening should force the reverse safe direction before pogo disengagement.

## Robustness goal

The mechanical interlock contract is the **preferred operating sequence**, but Bison should not depend on perfect fixture behavior for its own survival.

Design goal:

> **Bison should survive DUT insertion, removal, or partial contact even if the fixture interlock is missing, misadjusted, late, or ignored.**

Examples include:

- DUT removed while powered;
- pogo contact breaking before `FIXTURE_INTERLOCK` deasserts;
- partial/uneven pogo engagement;
- powered DUT contacting Bison signal pins before ground;
- signal pins momentarily shorting during fixture motion;
- bank logic voltage mismatched during accidental hot insertion;
- DUT externally powered while Bison believes DUT power is off.

This does not imply that the DUT must survive every misuse case.

The priority is that Bison's reusable hardware is not damaged by a fixture or DUT handling error.

## Electrical implications

Detailed implementation is deferred, but DUT-facing circuitry should be selected with hot-plug and fault tolerance in mind.

Relevant considerations include:

- translators with powered-off protection / Ioff behavior;
- current-limited or protected outputs;
- series resistance where signal integrity permits;
- input clamps and transient protection;
- no damaging back-power paths through unpowered banks;
- safe translator OE defaults;
- power-switch protection against abnormal DUT-side conditions;
- tolerance of partial-contact and ground-first/ground-last edge cases where practical.

These protections should be designed so they do not compromise Bison's measurement or protocol reliability.

## Design priority

The hierarchy is:

1. Normal fixture operation follows the interlock and sequencing contract.
2. Bison rapidly enters a safe state when the interlock opens.
3. If the fixture violates the contract, Bison should still survive.
4. DUT survival under abusive hot-swap conditions is desirable but not a primary design requirement.

This robustness requirement should be considered when selecting every DUT-facing translator, buffer, driver, contact emulator, analog front end, and power-path component.
