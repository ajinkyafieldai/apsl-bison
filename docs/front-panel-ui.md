# Bison Front-Panel UI Architecture

## Purpose

This document freezes the current Bison front-panel UI/UX architecture.

The goal is a minimal physical interface that communicates Bison/DUT execution state clearly without duplicating information that belongs in the web UI.

---

## 1. Front-panel philosophy

Bison is networked test equipment.

The physical front panel should answer only the questions that matter when standing in front of the unit:

- Is Bison booting, idle, transitioning, running, or faulted?
- Is DUT power available?
- Is the DUT currently energized / under test?
- Has Bison detected a hardware/infrastructure fault?

Detailed configuration, diagnostics, logs, and DUT pass/fail results belong in the web UI.

The front panel should not attempt to become a miniature test-results dashboard.

---

## 2. Human controls

The physical UI is intentionally minimal:

- one RGB illuminated anti-vandal momentary pushbutton;
- one recessed RESET pinhole.

No additional normal-operation buttons are currently required.

The state machine is sufficiently linear that one momentary control is enough.

---

## 3. Anti-vandal button

The anti-vandal switch is fully software-controlled.

It does not directly switch DUT power.

A press is interpreted as a request to transition the DUT execution state.

### 3.1 Button behavior by state

- In **Idle — No DUT Power**, button presses have no effect.
- In **Idle — DUT Power Available**, a press requests ARM / RUN.
- In **Arming**, further presses are ignored until the transition completes.
- In **Running**, a press requests STOP.
- In **Stopping**, further presses are ignored until the transition completes.
- In **Fault**, recovery semantics are intentionally not overloaded onto the button unless a future requirement explicitly needs it.

The button therefore behaves as:

> request the next valid state transition.

The current Bison state determines what the press means.

---

## 4. State machine

The frozen front-panel-visible Bison states are:

1. Booting
2. Bison Idle — No DUT Power
3. Bison Idle — DUT Power Available
4. Arming
5. Running
6. Fault
7. Stopping

Nominal flow:

```
BOOTING
   |
   v
IDLE_NO_POWER
   |
   | DUT power becomes valid
   v
IDLE_POWER
   |
   | button press
   v
ARMING
   |
   | success
   v
RUNNING
   |
   | button press
   v
STOPPING
   |
   v
IDLE_POWER
```

If DUT power disappears while idle, the state returns to `IDLE_NO_POWER`.

Faults may be entered asynchronously from relevant states.

---

## 5. RGB indication language

The anti-vandal RGB illumination is the primary front-panel state indicator.

### 5.1 State mapping

| State | RGB behavior |
| --- | --- |
| Booting | Cycle through all colors |
| Bison Idle — No DUT Power | Green breathing |
| Bison Idle — DUT Power Available | Green solid |
| Arming | Amber breathing |
| Running | Red solid |
| Fault | Red blink pattern |
| Stopping | Amber blinking |

### 5.2 Semantic language

The indication language is deliberately small:

- **Green** = idle / safe state
- **Amber** = transition in progress
- **Red** = DUT energized or Bison fault
- **Animation** distinguishes stable, transitional, and fault states

Booting is intentionally unique and cycles through colors.

---

## 6. Fault indication

Fault indication is for quick physical triage only.

A fault is a Bison / fixture / electrical / infrastructure fault, for example:

- DUT power fault;
- translator-domain overcurrent / contention;
- positive overvoltage / clamp fault;
- fixture interlock fault;
- Bison internal hardware fault.

The red blink pattern may encode a small fault class vocabulary.

The exact blink vocabulary is not frozen here.

The detailed cause, measurements, timestamps, and recovery information belong in the web UI / API / logs.

---

## 7. DUT test result is not a Bison fault

A critical UI rule is:

> DUT functional test pass/fail is not represented as a Bison front-panel fault.

A perfectly healthy Bison may complete a test in which the DUT fails.

The DUT test verdict is carried through the fixture/ribbon interface and is displayed and recorded by the web UI / test workflow.

The anti-vandal LED must not be repurposed later to show production PASS/FAIL.

Red fault indication means Bison or fixture infrastructure had a fault, not that the DUT failed its functional test.

---

## 8. RESET pinhole

The front panel includes one recessed pinhole labeled:

`RESET`

It follows the conventional networking-equipment / Wi-Fi-router interaction model.

RESET is for factory reset / network recovery, not for ordinary reboot.

The implementation should use a long-hold action so an accidental short press does not erase configuration.

No custom terminology should be invented for this feature.

---

## 9. Hard power vs DUT run control

Bison has two distinct power concepts.

### Rear mains control

The rear IEC power-entry module owns hard mains power.

It includes the hard AC power switch and associated fuse/filtering as selected during electrical design.

When rear mains power is OFF:

- Bison is truly off;
- Ethernet is down;
- the front RGB button is unpowered.

### Front anti-vandal control

The front anti-vandal switch controls the DUT execution state only.

Bison itself remains powered, booted, and network-connected while the DUT is idle.

This is intentionally similar to an appliance that remains network-connected while its controlled load is not active.

---

## 10. Ethernet indication

Ethernet link/activity indication should remain on the RJ45 connector where possible.

Do not duplicate link/activity with separate front-panel LEDs unless a concrete requirement emerges.

---

## 11. USB-C service connector

USB-C remains an internal/service interface.

The front-panel footprint may be reserved, but USB-C should not be populated on production units until the product's USB compliance / identity strategy is resolved.

The supported external/customer-facing interface remains Ethernet.

---

## 12. Front-panel connector set

Current front-panel connector/function set:

- DUT / fixture connector;
- Ethernet;
- USB-C service footprint, DNP until appropriate;
- RGB anti-vandal momentary pushbutton;
- recessed RESET pinhole.

No additional front-panel indicators are currently required.

---

## 13. Front-panel PCB implication

The UI is deliberately sparse.

Whether a dedicated front-panel PCB remains worthwhile should be decided mechanically rather than because the UI requires one.

If a separate front PCB is retained, it may still be useful for:

- anti-vandal wiring/control;
- RESET switch;
- front connector alignment;
- legends / branding;
- chassis / connector-shield bonding;
- clean card-edge interconnect to the main board.

If these do not justify a separate PCB, the main board may extend to the front and use a passive panel instead.

The UI architecture itself does not require a separate front-panel PCB.

---

## 14. Frozen UI requirements

The following are now frozen as the Bison front-panel UI baseline:

- one RGB illuminated anti-vandal momentary button;
- one recessed RESET pinhole;
- no separate READY, DUT, PWR, or FAULT LEDs;
- Ethernet link/activity remains on the RJ45;
- the anti-vandal button is software-controlled and does not directly switch power;
- button presses request the next valid DUT execution-state transition;
- seven visible states: Booting, Idle-No-Power, Idle-Power, Arming, Running, Fault, Stopping;
- RGB state mapping as defined above;
- front-panel fault indication is infrastructure fault triage only;
- DUT functional PASS/FAIL is carried through the fixture interface and shown in the web UI;
- rear IEC switch is the hard Bison power switch;
- front anti-vandal control is for DUT execution only;
- USB-C is service-only and may remain DNP on production hardware until the USB strategy is resolved.
