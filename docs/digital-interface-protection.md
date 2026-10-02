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

TXS/TXB-style automatic direction translators are not preferred for the general Bison fixture interface because their behavior depends heavily on DUT capacitance, pull-ups, and loading.

### 2.3 I2C

I2C remains a special case and should use an open-drain/bidirectional translation topology designed specifically for I2C.

---

## 3. Voltage banks

The DUT-facing translated I/O is divided into voltage domains controlled by VCCB rails.

```
RA6M3 side                    DUT side
   VCCA                         VCCB
    |                            |
    +------ translator ----------+
              |
              +---- DUT signals
```

VCCA is expected to be the RA6M3 logic rail.

VCCB defines the DUT-facing logic level.

The immediate requirement is reliable 3.3 V and 5 V operation. Other bank voltages may be considered later, but are not frozen by this document.

A bank voltage is configuration, not a live signal-routing function. Signals remain permanently connected to their assigned translator path.

---

## 4. Bank-voltage sequencing

Changing a bank voltage while the DUT is active is prohibited.

> A VCCB rail may only be changed while the DUT is unpowered and every translator using that rail is Hi-Z.

Expected startup/configuration sequence:

1. DUT power OFF.
2. All DUT-facing translator groups Hi-Z.
3. Configure the required VCCB rails.
4. Enable the bank regulators.
5. Wait for the rails to settle and, where practical, verify them.
6. Enable the required translator groups.
7. Enable DUT power.

Fixture removal follows the safe direction:

1. DUT power OFF.
2. Translators Hi-Z.
3. Contact emulators open.
4. Active protocol drivers disabled where applicable.
5. Fixture may be opened/removed.

This sequencing works together with the dedicated FIXTURE_INTERLOCK architecture documented separately.

---

## 5. What makes output contention dangerous

Bison does not need to detect the abstract condition "two outputs are connected."

If both outputs drive the same state, there is no meaningful fault.

The damaging case is opposing drive states:

```
Bison output: HIGH
DUT output:   LOW
```

The actual failure mechanism is excessive current through the output stages followed by excessive junction heating.

Therefore the useful protection quantity is current, not inferred logic-state disagreement.

---

## 6. Avoid blanket series resistance

A blanket series resistor on every DUT-facing digital line is not the preferred protection mechanism because it changes source impedance, edge shape, timing, and high-speed behavior.

Series resistance may still be used where required for signal-integrity reasons, but it should not be the primary Bison fault-protection mechanism.

Similarly, a generic analog mux/switch should not be inserted into every high-speed signal path unless it buys a specific protection capability worth the added RON, capacitance, bandwidth loss, and cost.

---

## 7. Current protection architecture

The current preferred architecture is to monitor the current consumed by a translator protection domain and use a hardware current fault to disable that domain.

```
                 +--------------------> current monitor / comparator
                 |
VCCB_SOURCE -- RSHUNT ----------------> translator protection domain
                                          |
                                          +---- DUT signals

current fault ---------------------------> hardware OE kill
```

The key architectural refinement is that the protection domain is not fixed to "one pin", "one package", or "one whole VCCB rail".

Instead, protection-domain width is a design knob.

---

## 8. Protection granularity is tunable

The design may use different translator widths depending on the required fault-detection margin and cost.

Examples:

- one single-channel translator behind one monitored feed -> per-pin protection
- one dual-channel translator -> two-pin protection domain
- one quad translator -> four-pin protection domain
- one 8-channel translator such as a 74x245 -> eight-pin protection domain
- multiple translator packages sharing one monitored VCCB feed -> wider protection domain

The target is not maximum granularity.

> Use the widest protection domain that still gives a clean separation between legitimate operating current and a damaging fault.

Per-pin protection is available if ever justified, but is expected to be too expensive for broad use.

An 8-channel domain is a sensible cost-oriented starting point, then the domain can be split only where the current budget or interface behavior requires it.

This gives Bison a clean optimization knob between:

- BOM cost
- protection sensitivity
- fault isolation
- package count
- routing complexity

---

## 9. Protection-domain sizing rule

The sizing criterion is:

```
I_NORMAL_MAX  <<  I_TRIP  <  I_DANGEROUS_FAULT
```

where:

- `I_NORMAL_MAX` is the worst-case legitimate aggregate current for the protection domain;
- `I_TRIP` is the hardware overcurrent threshold;
- `I_DANGEROUS_FAULT` is the current level at which the translator/output stage cannot safely tolerate the protection delay.

The domain width should be reduced when legitimate aggregate current becomes too close to the desired trip threshold.

For example:

- if eight channels together normally consume only a few tens of milliamps and one contention event pushes the domain well above 100 mA, an 8-channel domain is attractive;
- if eight channels can legitimately approach the trip threshold, split the domain into smaller groups.

The protection width is therefore determined by current-margin math, not by an arbitrary rule.

---

## 10. Hardware response

Protection must not depend on firmware reaction time.

```
contention / short
       |
       v
domain current rises
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
complete protection domain -> Hi-Z
```

The MCU also receives the fault indication for logging, test failure reporting, recovery policy, and deliberate re-arming.

The fault should remain latched until deliberately cleared rather than repeatedly oscillating into a persistent short.

A current-sense comparator with an integrated latch may eliminate the need for a separate latch.

---

## 11. INA301-class solution

The INA301 is currently a useful candidate/reference architecture because it combines:

- shunt current sensing
- amplification
- a fast overcurrent comparator
- approximately microsecond-class fault response
- alert output
- analog current-monitor output

The exact device is not frozen.

A roughly USD 1 protection IC is acceptable when amortized across a useful protection domain rather than used per individual pin.

This is one reason an 8-channel translated domain is attractive if its current margins work.

---

## 12. Preliminary current threshold

A threshold around 100 mA has been discussed as a plausible starting point.

This is not yet a frozen specification.

A 100 mA threshold is attractive only if:

1. valid operation remains comfortably below it;
2. one meaningful fault reliably drives the monitored current above it;
3. the translator can safely survive the fault current for the detector + latch + OE-disable delay.

Normal operation should have substantial margin below the trip point.

---

## 13. Translator survivability requirement

The translator does not need to survive a short indefinitely.

It must survive:

> the worst-case fault current for the complete protection reaction time.

For each selected translator verify:

- absolute maximum per-pin output current
- aggregate/package current limits
- output short-circuit behavior
- contention behavior where documented
- output impedance / expected short current
- transient thermal capability
- OE disable propagation time
- powered-off / partial-power-down behavior
- backfeed paths

The complete protection delay is approximately:

```
current detector response
+ latch / logic propagation
+ translator OE disable delay
```

The resulting transient stress must be demonstrably safe.

---

## 14. Precision is not the primary requirement

The protection circuit is primarily answering:

> Is current unquestionably outside valid operation?

Exact current measurement accuracy is secondary to fast and predictable trip behavior.

An analog current-monitor output is useful but optional.

A cheaper solution remains acceptable if it gives adequate threshold accuracy and fault response.

---

## 15. PTC discussion

A PTC/PPTC was considered as a passive protection mechanism.

It is attractive as a passive backstop, but it is not sufficient as the primary protection method because trip behavior is thermal and relatively slow.

A PTC could potentially protect a monitored VCCB feed as a last-resort passive mechanism, but fast electronic detection followed by OE shutdown remains the primary design direction.

---

## 16. Overvoltage is a separate problem

Current-domain protection does not by itself protect a DUT-facing signal against an externally applied excessive voltage.

Example:

- a DUT or fixture accidentally applies 12 V to a Bison digital pin.

That fault enters through the signal pin rather than through the monitored VCCB source.

Therefore the design treats:

### Contention / short protection

with:

- monitored VCCB current
- hardware overcurrent detection
- asynchronous OE shutdown

### External signal overvoltage

as a separate design problem requiring consideration of:

- translator pin absolute maximum ratings
- powered-off tolerance
- internal clamp structures
- fault-protected switches/buffers where justified
- external clamps / transient protection
- protocol-specific protection

---

## 17. Reverse-current and externally powered DUT cases

A DUT can remain externally powered even when Bison has disabled DUT power.

The selected translator/protection topology must explicitly handle:

- externally powered DUT
- Bison bank unpowered while DUT drives a pin
- Bison powered while DUT bank is unpowered
- fixture insertion/removal with one side powered
- partial contact

Ioff / partial-power-down behavior is therefore mandatory to evaluate.

Current sensing on the translator VCCB feed may not observe every possible reverse-energy path from the DUT, so reverse-current/backfeed behavior must be checked separately.

---

## 18. High-speed interfaces require the same protection

UART, SPI, and high-speed DUT-facing GPIO require protection as well as slow GPIO.

The current-domain architecture is attractive because the high-speed signal path can remain extremely simple:

```
MCU -> translator -> DUT
```

The protection sensing lives on the translator supply rather than in series with every signal.

This preserves signal integrity while still allowing hardware shutdown on sustained contention.

---

## 19. Fault granularity

A current fault disables the complete protection domain associated with the monitored feed.

This is intentional.

The design does not currently require identification of the exact offending pin in hardware.

Software can use knowledge of the operation that was active when the trip occurred to aid diagnosis.

Protection-domain width may be tuned from one channel to eight channels or more depending on cost and current margin.

---

## 20. Current architecture summary

```
                 VCCB_SOURCE
                     |
                   RSHUNT
                     |
              +------+------+
              |             |
              |       current-sense
              |        comparator
              |             |
              v             v
      translator group    FAULT
              |             |
              |             +------> MCU fault input
              |             |
              |             +------> latch / hardware OE kill
              |
              v
         DUT / fixture
```

The protection philosophy is:

> Detect abnormal aggregate current quickly, force the associated translator group Hi-Z in hardware, and keep the high-speed signal path as simple as possible.

---

## 21. Items still to verify

Before freezing the implementation:

1. Select translator family members and useful widths.
2. Characterize or calculate worst-case contention current.
3. Establish worst-case legitimate current for each proposed protection domain.
4. Determine whether 8-channel domains work for most interfaces.
5. Split to 4/2/1-channel domains only where required.
6. Select a provisional trip threshold.
7. Verify detector response at that threshold.
8. Verify latch/OE propagation.
9. Verify translator OE disable time.
10. Calculate transient power/energy during a worst-case fault.
11. Verify package-level aggregate current limits.
12. Verify reverse-drive / externally powered DUT behavior.
13. Decide whether passive backup protection is worthwhile.
14. Design the separate DUT-side overvoltage protection strategy.

---

## 22. Design decisions captured here

Current decisions/directions:

- RA6M3 pins are never directly exposed to the DUT.
- Push-pull translation uses deterministic direction-controlled dual-supply translators.
- 74x245-class parts are the current translation direction.
- Auto-direction translators are not the preferred general solution.
- VCCB voltage cannot change while the DUT is powered or translators are enabled.
- Blanket series resistance is not the primary protection method.
- Output contention is treated as an overcurrent/thermal problem.
- Current protection is applied per translator protection domain, not necessarily per pin or per whole voltage rail.
- Protection-domain width is a tunable design knob.
- Use the widest domain that preserves a clear fault-current margin.
- Per-pin protection is available but not the default because it would inflate cost.
- 8-channel protection domains are a sensible starting point where the current budget allows.
- Fault response must be hardware-driven and force the affected translator group Hi-Z.
- Approximately USD 1 of protection electronics per useful domain is acceptable.
- INA301-class fast current-sense/comparator devices are credible candidates.
- A roughly 100 mA trip level is a discussion starting point, not a frozen requirement.
- High-speed interfaces require protection as well as slow GPIO.
- Signal overvoltage is a distinct problem from overcurrent and remains to be solved separately.
