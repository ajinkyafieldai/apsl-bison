# DUT Digital Interface Translation and Protection

## Purpose

This document captures the current Bison design direction for DUT-facing digital I/O translation and protection.

The central design goal is:

> No RA6M3 pin is connected directly to the DUT or fixture.

Bison is reusable lab/test equipment. DUTs and interposers are comparatively replaceable. The DUT-facing electrical architecture must therefore protect Bison from fixture mistakes, hot insertion/removal, output contention, wrong voltage domains, partial contact, and other realistic abuse without unnecessarily degrading high-speed digital signals.

This document is intentionally about the common digital front end. Protocol-specific physical layers such as isolated CAN remain separate.

---

## 1. Protection boundary

Every DUT-facing MCU signal passes through an interface stage appropriate to its function.

Examples:

- UART, SPI, and push-pull GPIO: deterministic dual-supply level translator/buffer
- I2C: dedicated open-drain/bidirectional translation
- CAN: isolated CAN transceiver
- Analog measurement: protected analog front end
- Contact emulation: PhotoMOS/relay-class isolated switch
- Other protocol-specific interfaces: appropriate dedicated transceiver

The RA6M3 must never be treated as the fixture protection element.

Conceptually:

```
RA6M3
  |
  v
Bison interface / translation / protection layer
  |
  v
Ribbon / interposer / pogo pins
  |
  v
DUT
```

---

## 2. Translation philosophy

Bison must work across varied DUTs and fixture loading. The design therefore prefers deterministic, direction-controlled translation over auto-direction devices.

### 2.1 Push-pull interfaces

The current preferred primitive is a 74x245-class dual-supply translator, specifically the 74LVC8T245 / equivalent family as a starting point.

Desired properties:

- separate VCCA and VCCB
- explicit DIR
- explicit OE / Hi-Z
- partial-power-down / Ioff behavior
- fixed signal routing
- deterministic direction
- adequate bandwidth for UART, SPI, and high-speed GPIO
- DUT-side supply compatible with the selected bank voltage

The exact manufacturer and device remain to be selected after electrical verification.

### 2.2 Auto-direction translators

TXS/TXB-style automatic direction translators are not preferred for the general Bison fixture interface.

Reasons include:

- behavior depends strongly on DUT capacitance and pull-ups
- loading varies between DUTs and interposers
- edge accelerators / one-shots can create difficult-to-predict behavior
- they do not provide the deterministic interface contract wanted for reusable test equipment

### 2.3 I2C

I2C is a special case and should use an open-drain/bidirectional translation topology designed specifically for I2C.

Do not force I2C through the push-pull 74x245 architecture.

---

## 3. Voltage banks

The DUT-facing translated I/O is divided into voltage banks.

For a push-pull bank:

```
RA6M3 side                    DUT side
   VCCA                         VCCB
    |                            |
    +------ 74x245 bank --------+
              |
              +---- DUT signals
```

VCCA is expected to be the RA6M3 logic rail.

VCCB is generated per bank and defines the DUT-facing logic level for that bank.

The immediate requirement is reliable 3.3 V and 5 V operation. Other bank voltages may be considered later, but are not frozen by this document.

A bank voltage is configuration, not a live signal-routing function. Signals remain permanently connected to their assigned translator path.

---

## 4. Bank-voltage sequencing

Changing a bank voltage while the DUT is active is prohibited.

Hard invariant:

> A bank VCCB may only be changed while the DUT is unpowered and the corresponding translators are Hi-Z.

Expected startup/configuration sequence:

1. DUT power OFF.
2. All DUT-facing translator banks Hi-Z.
3. Configure the required VCCB for each bank.
4. Enable the bank supply/regulator.
5. Wait for the rail to settle and, where practical, verify it.
6. Enable the required translator banks.
7. Enable DUT power.

Fixture removal follows the corresponding safe direction:

1. DUT power OFF.
2. Translators Hi-Z.
3. Contact emulators open.
4. Active protocol drivers disabled where applicable.
5. Fixture may be opened/removed.

This sequencing works together with the dedicated FIXTURE_INTERLOCK architecture documented separately.

---

## 5. What actually makes output contention dangerous

Bison does not need to detect the abstract condition "two outputs are connected."

Output-to-output connection is harmless if both drivers produce the same state.

The damaging case is opposing drive states, for example:

```
Bison output: HIGH
DUT output:   LOW
```

The relevant failure mechanism is:

> excessive current through the output stages, followed by excessive junction heating.

Therefore the useful protection quantity is current, not inferred logic-state disagreement.

This is an important simplification.

---

## 6. Avoid blanket series resistance

A blanket series resistor on every DUT-facing digital line is not the preferred protection mechanism.

Reasons:

- it changes source impedance
- it alters edge shape
- it interacts with fixture/cable capacitance
- it complicates timing
- it can reduce usable SPI/high-speed GPIO performance
- the required resistance for meaningful short-circuit protection may be much larger than desirable for signal integrity

Series resistance can still be used where required for signal-integrity reasons, but it should not be the primary Bison fault-protection strategy.

Likewise, putting a generic analog mux/switch in every signal path purely for protection is not currently preferred because the additional RON, capacitance, bandwidth limits, and cost must be justified by a real protection benefit.

---

## 7. Bank-level overcurrent detection

The current preferred protection architecture is to monitor the supply current of each push-pull translator bank.

Concept:

```
                 +--------------------> current monitor / comparator
                 |
VIO_BANK ---- RSHUNT -----------------> 74x245 VCCB
                                          |
                                          +---- multiple DUT signals

current fault ---------------------------> hardware OE kill
```

A single current-sense channel therefore protects a complete translator bank rather than an individual I/O.

This is important both technically and economically.

### 7.1 Why bank-level sensing works

When a Bison output is:

- shorted to ground
- shorted to a conflicting rail
- driven against by a DUT output
- involved in another fault causing excessive source current

the translator bank supply current rises.

If the aggregate bank current exceeds a threshold that cannot occur during valid operation, the bank is declared faulted.

The protection does not need to identify the offending pin.

Once a fault exists, the appropriate action is to safe-state the complete bank.

---

## 8. Hardware response

Protection must not depend on firmware reaction time.

The intended path is:

```
contention / short
       |
       v
bank current rises
       |
       v
fast current comparator
       |
       v
hardware latch
       |
       v
translator OE -> disabled
       |
       v
complete bank -> Hi-Z
```

The MCU also receives the fault indication for:

- logging
- test failure reporting
- determining recovery policy
- clearing/rearming the protection when appropriate

The fault should remain latched until deliberately cleared rather than repeatedly oscillating into a persistent short.

A current-sense comparator with an integrated latch may remove the need for a separate external latch.

---

## 9. INA301-class solution

The INA301 is currently a useful candidate/reference architecture because it combines:

- shunt current sensing
- amplification
- a fast overcurrent comparator
- approximately microsecond-class fault response
- a comparator/alert output
- analog current-monitor output

The exact device is not frozen.

The important architectural point is that a roughly USD 1 protection device is acceptable when it protects an entire bank rather than one signal.

For example, one protection channel supervising an eight-bit translator bank amortizes the cost across eight DUT-facing signals.

A cheaper discrete comparator solution may still be considered, but cost pressure is significantly lower at bank granularity.

---

## 10. Preliminary current threshold

A threshold around 100 mA has been discussed as a plausible starting point.

This is not yet a frozen specification.

The desired relationship is:

```
maximum legitimate aggregate bank current
          <<

protection threshold
          <

current / energy capable of damaging the translator during the protection delay
```

A 100 mA threshold is attractive only if:

1. valid operation remains comfortably below it;
2. the translator can safely survive the fault current for the detector + latch + OE-disable delay;
3. the protection path reliably disables the bank before damaging thermal energy accumulates.

Normal operating current should have substantial margin below the trip point. The design should not rely on distinguishing, for example, 90 mA normal operation from a 100 mA fault.

---

## 11. Translator survivability requirement

This architecture does not require the translator to survive an output short indefinitely.

It requires the translator to survive:

> the worst-case fault current for the complete protection reaction time.

For each selected translator the design must verify:

- absolute maximum per-pin output current
- aggregate/package current limits
- output short-circuit behavior
- contention behavior where documented
- output impedance / expected short current
- thermal transient capability
- OE disable propagation time
- powered-off / partial-power-down behavior
- backfeed paths

The complete protection delay is approximately:

```
current detector response
+ latch / logic propagation
+ translator OE disable delay
```

The resulting transient electrical and thermal stress must be demonstrably safe.

---

## 12. Why precision is not the main requirement

The protection circuit is primarily answering a binary question:

> Is bank current unquestionably outside normal operation?

It is not intended to be precision instrumentation.

Therefore:

- exact current measurement accuracy is secondary
- fast and predictable trip behavior is more important
- threshold tolerance only needs to support a clear gap between valid and fault current
- an analog current-monitor output is useful but optional

If a future cheaper solution provides an adequate fast comparator without precision current telemetry, it remains a valid candidate.

---

## 13. PTC discussion

A PTC/PPTC was considered as a passive protection mechanism.

A PTC responds to sustained overcurrent by self-heating and transitioning to a high-resistance state.

It is attractive as a passive backstop, but it is not currently considered sufficient as the primary protection method because:

- trip behavior is thermal
- trip time is relatively slow
- a translator must survive the initial fault until the PTC heats
- placing a PTC in each signal path adds series impedance and can affect high-speed behavior

A PTC could potentially be used on a bank supply as a last-resort passive protection mechanism, but the primary design direction is fast electronic current detection followed by OE shutdown.

---

## 14. Overvoltage is a separate problem

Bank overcurrent protection does not by itself protect a DUT-facing signal against an externally applied excessive voltage.

Example:

- a DUT or fixture accidentally applies 12 V to a Bison digital pin.

That fault enters through the signal pin rather than the bank VCCB feed.

Therefore the design separates:

### Contention / short protection

Handled primarily by:

- bank-current monitoring
- hardware fault detection
- asynchronous OE disable

### External signal overvoltage

Requires separate consideration of:

- translator input/output absolute maximum ratings
- powered-off tolerance
- clamp structures
- fault-protected signal switches/buffers where justified
- protocol-specific protection
- external transient/ESD protection

Fault-protected analog switches were investigated as one possible DUT-edge element, but they are not currently mandatory because they often solve overvoltage faults without solving same-voltage output contention, while adding RON and capacitance to the high-speed path.

---

## 15. Reverse-current and externally powered DUT cases

A DUT can remain externally powered even when Bison has disabled DUT power.

The design therefore cannot assume that turning off Bison's DUT supply removes voltage from DUT-facing signals.

The selected translator/protection topology must explicitly handle:

- externally powered DUT
- Bison bank unpowered while DUT drives a pin
- Bison powered while DUT bank is unpowered
- fixture insertion/removal with one side powered
- partial contact

Ioff / partial-power-down behavior is therefore a mandatory translator-selection consideration.

Bank supply current sensing is excellent for current sourced by Bison. It may not observe every possible reverse-energy path from the DUT, so reverse-current/backfeed behavior must be checked separately during device selection.

---

## 16. High-speed interfaces require the same protection

Protection is not limited to slow generic GPIO.

UART, SPI, and high-speed DUT-facing GPIO must also survive realistic fixture faults.

The selected architecture is deliberately attractive here because:

- the normal signal path remains only the translator
- no protection resistor is required purely for overcurrent protection
- no analog switch is necessarily required in series
- the fast current detector is on the bank supply, not the signal path
- hardware OE shutdown can be asynchronous to firmware

This preserves the best chance of maintaining clean high-speed digital behavior while still protecting Bison from sustained contention.

---

## 17. Fault granularity

A current fault on one signal disables the entire associated voltage bank.

This is intentional.

Once one DUT/fixture connection is electrically invalid, continuing to drive adjacent signals in that bank is not useful enough to justify per-pin protection complexity.

Software may later use knowledge of the operation being performed when the trip occurred to identify the likely offending signal.

No requirement currently exists for per-pin current sensing.

---

## 18. Current architecture summary

For a push-pull DUT-facing bank:

```
                 BANK_VIO
                    |
                  RSHUNT
                    |
             +------+------+
             |             |
             |       current-sense
             |        comparator
             |             |
             v             v
           VCCB          FAULT
             |             |
        +---------+        +------> MCU fault input
MCU --->| 74x245  |        |
        +---------+        +------> latch / hardware OE kill
             |
             v
        DUT / fixture
```

The protection philosophy is:

> Detect abnormal aggregate translator current quickly, force the complete bank Hi-Z in hardware, and keep the high-speed signal path as simple as possible.

---

## 19. Items still to verify

Before freezing the implementation:

1. Select the exact 74x245-family translator.
2. Characterize or calculate worst-case contention current.
3. Establish realistic maximum normal bank current.
4. Select a provisional trip threshold.
5. Verify detector response time at that threshold.
6. Verify latch/OE logic propagation.
7. Verify translator OE disable time.
8. Calculate transient power/energy during a worst-case fault.
9. Verify package-level aggregate current limits.
10. Verify behavior for reverse drive / externally powered DUT.
11. Decide whether any passive backup protection is worthwhile.
12. Determine whether particular interfaces need additional DUT-edge overvoltage protection.
13. Determine bank partitioning based on voltage domains, direction groups, expected current, and interface assignment.

---

## 20. Design decisions captured here

Current decisions/directions:

- RA6M3 pins are never directly exposed to the DUT.
- Push-pull translation uses deterministic direction-controlled dual-supply translators.
- 74x245-class parts are the current translation direction.
- Auto-direction translators are not the preferred general solution.
- DUT-facing translated signals are grouped into voltage banks.
- Bank voltage cannot be changed while the DUT is powered or translators are enabled.
- Blanket series resistance is not the primary protection method.
- Output contention is treated as an overcurrent/thermal problem.
- Overcurrent detection is bank-level, not per-I/O.
- Fault response must be hardware-driven and force the bank Hi-Z.
- Approximately USD 1 of protection electronics per bank is acceptable.
- INA301-class fast current-sense/comparator devices are credible candidates.
- A roughly 100 mA trip level is a discussion starting point, not a frozen requirement.
- High-speed interfaces require protection as well as slow GPIO.
- Signal overvoltage is a distinct problem from bank overcurrent and remains to be solved/verified separately.
