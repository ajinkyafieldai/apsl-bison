# Bison Front-Panel UI Architecture

## Purpose

This document freezes the current Bison front-panel UI/UX architecture.

The goal is a minimal physical interface that communicates Bison/DUT execution state clearly without duplicating information that belongs in the web UI, while remaining understandable without relying on color alone.

---

## 1. Front-panel philosophy

Bison is networked test equipment.

The physical front panel should answer only the questions that matter when standing in front of the unit:

- Is Bison ready, transitioning, running, or faulted?
- Is Bison moving toward ACTIVE or back toward READY?
- Has Bison detected a hardware/infrastructure fault?

Detailed configuration, diagnostics, logs, and DUT pass/fail results belong in the web UI.

The front panel should not become a miniature test-results dashboard.

---

## 2. Human controls and indicators

The physical UI is intentionally minimal:

- one plain black momentary anti-vandal pushbutton;
- one recessed RESET pinhole;
- three labelled status LEDs: READY, ACTIVE, FAULT.

The anti-vandal button is not itself the primary state indicator.

The separate labelled LEDs were chosen so state can be understood from position, label, and animation direction rather than color alone.

---

## 3. Anti-vandal button

The anti-vandal switch is software-controlled.

It does not directly switch DUT power.

A press is interpreted as a request to transition the DUT execution state.

### 3.1 Button behavior by state

- In **Pre-operational / no valid DUT power**, button presses have no effect.
- In **Ready**, a press requests START.
- During **Starting**, further presses are ignored until the transition completes.
- In **Running**, a press requests STOP.
- During **Stopping**, further presses are ignored until the transition completes.
- In **Fault**, recovery semantics are not overloaded onto the normal start/stop action; fault reset follows the defined recovery path.

The button therefore behaves as:

> request the next valid operator transition.

The current Bison state determines what the press means.

---

## 4. Operator-visible state vocabulary

The front-panel vocabulary is intentionally compact:

- READY
- ACTIVE
- FAULT

The underlying firmware may have more detailed internal states, but the front panel communicates them through these three labelled indicators and their animations.

The simplified operator-facing flow is:

```
READY <-> ACTIVE
          |
          v
        FAULT
          |
          v
        READY
```

The arrows on the front panel are intentional:

- opposing half-arrows between READY and ACTIVE show that the transition is bidirectional;
- motion from READY toward ACTIVE means starting;
- motion from ACTIVE toward READY means stopping;
- ACTIVE -> FAULT indicates that a running/active system may enter fault;
- FAULT -> READY indicates successful fault reset returns to the safe ready state.

This is an operator-facing simplification, not the exhaustive firmware transition graph.

---

## 5. LED indication and animation language

| State | Indication |
| --- | --- |
| Booting | LEDs cycle in sequence |
| Pre-operational | READY: green sinusoidal breathing |
| Idle / Ready | READY: steady green |
| Starting | Brightness sweeps from READY to ACTIVE |
| Running | ACTIVE: steady red |
| Stopping | Brightness sweeps from ACTIVE to READY |
| Error | FAULT: red blinking, with product-specific quick-reference error pattern |
| Resetting error | FAULT: red sinusoidal breathing; successful reset returns to READY |

Starting uses a directional sweep across READY and ACTIVE so the animation itself communicates motion toward the active state.

Stopping reverses the sweep.

The exact PWM levels and timing are tuned on hardware. ACTIVE must support the transition color behavior and steady red running state required by the product vocabulary.

---

## 6. Accessibility rule

The front panel must not require color discrimination to interpret the primary state.

State is communicated redundantly through:

- labelled LED position;
- which indicator is active;
- animation type;
- animation direction;
- front-panel arrows.

Color is supplementary.

---

## 7. Fault indication

FAULT indication is for quick physical triage only.

A fault is a Bison / fixture / electrical / infrastructure fault, for example:

- DUT power fault;
- translator-domain overcurrent / contention;
- positive overvoltage / clamp fault;
- fixture interlock fault;
- Bison internal hardware fault.

The red blink pattern may encode a small fault-class vocabulary.

The exact fault-code mapping may be finalized later.

Detailed cause, measurements, timestamps, and recovery information belong in the web UI / API / logs.

---

## 8. DUT test result is not a Bison fault

A critical UI rule is:

> DUT functional test pass/fail is not represented as a Bison front-panel fault.

A perfectly healthy Bison may complete a test in which the DUT fails.

The DUT test verdict is carried through the fixture interface and is displayed and recorded by the web UI / test workflow.

The READY / ACTIVE / FAULT indicators must not be repurposed to show production PASS/FAIL.

FAULT means Bison or fixture infrastructure had a fault, not that the DUT failed its functional test.

---

## 9. RESET pinhole

The front panel includes one recessed pinhole labelled:

`RESET`

It follows the conventional networking-equipment / Wi-Fi-router interaction model.

RESET is for factory reset / network recovery, not ordinary start/stop operation.

The implementation should use a long-hold action so an accidental short press does not erase configuration.

---

## 10. Hard power vs DUT run control

Bison has two distinct power concepts.

### Rear mains control

The rear IEC power-entry module owns hard mains power.

When rear mains power is OFF:

- Bison is truly off;
- Ethernet is down;
- the front indicators are unpowered.

### Front operator control

The front anti-vandal button controls the DUT execution state only.

Bison itself remains powered, booted, and network-connected while the DUT is idle.

---

## 11. Ethernet indication

Ethernet link/activity indication should remain on the RJ45 connector where possible.

Do not duplicate link/activity with separate front-panel LEDs unless a concrete requirement emerges.

A black visible RJ45 housing/bezel is preferred where practical, without compromising shield/chassis bonding.

---

## 12. Front-panel connector set

The front panel is frozen to the following user-facing elements:

- Ethernet;
- DUT / fixture DB25 ports;
- plain black momentary anti-vandal pushbutton;
- READY / ACTIVE / FAULT status LEDs;
- recessed RESET pinhole;
- DUT POWER IN barrel jack;
- DUT POWER OUT 2-position 5.08 mm pluggable screw-terminal system.

The exact number of DB25 ports is not frozen. Four ports are the current upper-bound packaging estimate for enclosure sizing.

DB25 is the Bison-side fixture connector. What cable technology the customer uses downstream is a fixture choice; IDC ribbon is one valid option, not a Bison contract.

### 12.1 DUT-power input

The front barrel jack is the dedicated DUT-power input.

Its presence is intentionally separate from Bison's own mains power entry.

The barrel jack is an input to Bison's controlled DUT-power path; it does not directly energize the DUT.

### 12.2 DUT-power output

The controlled DUT-power output uses a 2-position, 5.08 mm pluggable screw-terminal system.

The preferred visual treatment is orange + black.

The front-panel marking must clearly identify the output as `DUT POWER OUT` and mark `+` and `-` polarity adjacent to the two positions.

The exact final manufacturer part numbers may be frozen during detailed component selection.

### 12.3 Reverse-polarity protection

Connector selection does not eliminate polarity-reversal faults because the customer constructs the fixture cable.

The DUT power architecture must explicitly handle:

1. reversed DUT POWER IN at the barrel jack;
2. reversed DUT POWER OUT wiring caused by a customer cable/fixture.

The pre-power check now includes reverse-polarity validation before full-power enable.

This check is an additional preventive layer; Bison survival remains the hard requirement if incorrect wiring escapes the precheck.

The final protection circuit must prevent destructive back-power paths through Bison sensing, signal-ground, clamp, translator, or other interface circuitry.

DUT survival is not guaranteed.

---

## 13. Rear service interface

USB-C remains a service/debug interface and is placed on the rear panel.

Ethernet remains the supported customer-facing host interface.

---

## 14. Front-panel construction

There is no front-panel PCB.

Use the aluminum front plate supplied with the enclosure, machined for connectors/controls and silk-screened for legends, arrows, polarity, branding, and status labels.

The main PCB and chassis-mounted components should meet the front panel directly as appropriate.

---

## 15. Frozen UI requirements

The current Bison front-panel baseline is:

- plain black momentary anti-vandal button;
- READY / ACTIVE / FAULT labelled LEDs;
- color-blind-accessible state communication through labels, position, animation, and directional arrows;
- recessed RESET pinhole;
- Ethernet;
- DB25 DUT/fixture ports, exact count deferred, four used as current size estimate;
- DUT POWER IN barrel jack;
- DUT POWER OUT orange/black 2-position 5.08 mm pluggable screw terminal;
- USB-C service connector on the rear;
- no separate PASS/FAIL indication for DUT functional test result;
- rear IEC switch is the hard Bison power switch;
- aluminum machined and silk-screened front plate, with no front-panel PCB.
