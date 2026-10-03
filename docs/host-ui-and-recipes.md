# Bison host UI, onboarding and recipe architecture

Date: **3 October 2026**

Status: **Preparation phase — product and software architecture decisions recorded; recipe syntax and editor implementation remain open.**

This record captures the current host-side product model for Bison: the out-of-box connection experience, local web UI boundary, recipe ownership and storage model, run presentation, and the canonical tests that will be used to determine how expressive the recipe language must be.

The intent is to settle the product architecture before choosing a visual recipe editor, scripting language or DSL syntax.

## Product principle

Bison is an appliance and test instrument, not a generic web dashboard.

The normal user should be able to take a new Bison from the box, connect it to a computer and reach the instrument without understanding DHCP, mDNS, link-local addressing, subnet masks, serial ports or service USB.

The host experience must therefore begin with the connection contract, not with screen layout.

## Out-of-box connection experience

The normal first-use path is:

1. Power Bison.
2. Connect Bison Ethernet directly to the user's computer.
3. If the computer has no Ethernet port, use a qualified USB-to-Ethernet adapter.
4. Open **`bison.local`**.
5. Bison presents the local live UI.

The normal quick-start should not require manual network configuration.

### Ethernet startup behavior

Infra Ethernet is the customer-facing host interface.

On link-up Bison should converge automatically to a usable IPv4 configuration:

```text
Ethernet link up
      |
      v
Attempt DHCP
      |
      +-- lease acquired --> use DHCP address
      |
      +-- no DHCP --------> use IPv4 link-local
                                |
                                v
                         advertise mDNS
                                |
                                v
                           bison.local
```

mDNS should be available regardless of whether the address came from DHCP or link-local.

The same behavior should occur if Ethernet is connected after Bison has already booted.

The user-facing contract is therefore simply:

> Connect Ethernet and open `bison.local`.

Network troubleshooting is support documentation, not part of ordinary onboarding.

### Host-side link-local assumption

For the direct computer-to-Bison case, the supported OOBE assumes that a newly attached Ethernet interface with no DHCP server will fall back to host-side IPv4 link-local behavior.

This assumption must be qualified on the supported operating-system matrix and with the qualified USB-to-Ethernet accessory.

At minimum, acceptance testing should cover current supported Windows, macOS and Ubuntu configurations for:

- cold plug;
- hot plug;
- direct Bison connection;
- DHCP absence and link-local fallback;
- `bison.local` resolution;
- cable unplug/replug;
- computer sleep/resume.

The product documentation should teach manual network configuration only as a troubleshooting escape hatch.

## Service USB is not part of normal onboarding

Bison's USB-C device interface remains **service-only** while Bison does not have an appropriate customer-facing VID/PID allocation.

The normal host experience must not depend on USB networking or a USB driver.

This preserves a simple boundary:

- Ethernet: product/host interface;
- USB-C device: service interface.

## USB-to-Ethernet accessory direction

Modern laptops cannot be assumed to include an RJ45 port.

Bison should therefore qualify a known USB-to-Ethernet implementation rather than require the customer to discover and debug an arbitrary consumer dongle.

The current product direction is to use a known-good adapter implementation, remove dependence on the consumer shell/branding, and qualify the actual electronics as a Bison accessory.

Bison can exercise the Ethernet side of the accessory during production or service testing. Host USB enumeration and driver behavior still require operating-system qualification.

This accessory decision does not require 2.5 GbE; ordinary Ethernet performance is sufficient for Bison.

A separate possible accessory is a power-only adapter that converts Bison **DUT POWER OUT** to a USB power connector for USB-powered DUTs. This is distinct from the host Ethernet accessory and should not imply USB data or USB-PD support unless separately specified.

## Multiple Bison units

The default OOBE is optimized for one Bison.

`bison.local` is the normal first-device entry point. Multi-Bison installations are an advanced deployment/commissioning case and should not complicate the standard user flow.

Every Bison should nevertheless have a permanent unique identity suitable for individual addressing, inventory and commissioning. The exact hostname convention remains an implementation detail.

Advanced multi-unit deployments can be commissioned with support rather than exposing network-management complexity to every user.

## Host UI architecture

The first page is **Live**.

The most important persistent workspace is **Recipes**.

A useful top-level information architecture is:

```text
LIVE      RECIPES      RUNS      BISON
```

Names may change during UI design, but the conceptual separation should remain.

### Live

Live is the appliance view and must be available directly from Bison.

It should show the state of the currently connected DUT and the currently executing recipe/run, including:

- Bison state;
- DUT power state;
- voltage/current/power measurements;
- active faults and warnings;
- current recipe identity;
- execution progress;
- CI-style structured logs;
- start/stop/reset controls where permitted;
- relevant live signal/protocol measurements.

The firmware owns authoritative device state. The browser requests actions and renders reported state; it must not maintain a competing state machine.

### Runs

A recipe run should be presented like a CI execution trace rather than as a generic instrumentation dashboard.

Example:

```text
✓ Load recipe: Motor Controller Production Test
✓ Configure DUT power: 12.0 V / 4.0 A
✓ Configure CAN @ 500 kbit/s
✓ Flash firmware: production.bin
✓ Enable DUT power
✓ Wait for BOOT_OK
  └─ BOOT_OK asserted after 842 ms
✓ Expect CAN heartbeat within 2 s
  └─ Received ID 0x181 after 317 ms
✓ Assert START
✗ Verify motor current < 3.0 A
  └─ Measured 3.42 A
  └─ Limit 3.00 A
○ Remaining steps skipped
```

Steps should be expandable to expose timestamps, resolved inputs, measurements, captures, protocol data, flashing output and failure context.

Historical runs must preserve enough information to establish exactly which recipe and assets produced the result.

## Hybrid web application

Bison should not be forced to carry a large engineering web application in MCU flash.

The preferred architecture is hybrid.

### Device-resident UI

Bison serves a compact local web application containing everything required to use the appliance offline:

- Live view;
- current recipe/run;
- CI-style logs;
- measurements and faults;
- start/stop/reset;
- existing recipe selection and execution;
- recent/local run inspection;
- device/network/firmware basics.

The local bundle should remain deliberately lean and version-matched to the firmware.

### Internet-hosted engineering UI

Heavier authoring functionality may be delivered from the APSL-hosted web application:

- create/edit recipes;
- visual programming and/or advanced text editor;
- validation and linting;
- recipe/library management;
- Git operations and history;
- flash/template asset management;
- richer analysis tools.

The browser can load the engineering application from the Internet while communicating locally with Bison. Recipe data does not need to transit an APSL server merely because the editor was downloaded from one.

The important product boundary is:

> Bison carries everything required to run and observe tests. The richer online application is the engineering workstation used to author and manage them.

Loss of Internet access must not prevent an already provisioned Bison from running existing recipes.

## Recipe definition

A Bison recipe is the complete executable test definition for a DUT, not merely a settings preset.

A recipe may include:

- all Bison hardware configuration;
- channel and signal definitions;
- DUT power configuration and sequencing;
- ordered actions;
- causes/triggers;
- expected effects;
- timing requirements and timeouts;
- pass/fail conditions;
- protocol configuration and transactions;
- flash/programming templates or other run assets;
- parameters resolved for a particular run;
- logging/capture requirements;
- operator interaction steps.

The exact authoring paradigm remains open. It may be:

- visual programming;
- direct text/scripting;
- or a combination in which a visual editor round-trips a canonical text representation.

## Recipe source of truth

Recipes should be **plain-text, Git-versioned project artifacts**.

Git is the version-control backend and project repository is the source of truth.

Bison may cache recipes and must retain the exact recipe snapshot used for a run, but the instrument itself is not the authoritative long-term recipe database.

Conceptually:

```text
Project / Git
    |
    v
Host UI selects or edits recipe
    |
    v
Bison receives executable recipe package
    |
    v
Bison executes
    |
    v
Run record preserves exact recipe + resolved assets + results
```

The browser should not hold authoritative engineering content in local browser storage.

### Recipe package

A recipe may need associated flash images/templates and other assets, so treat the logical recipe as a package/directory even if the executable definition itself is one text file.

Illustrative project layout:

```text
project/
└── bison/
    └── recipes/
        └── motor-bringup/
            ├── recipe.<text-format>
            ├── assets/
            │   ├── bootloader.bin
            │   └── application.bin
            └── README.md
```

The actual file extension and syntax are intentionally not selected yet.

Past run records should preserve recipe identity/hash, resolved parameters and asset hashes so results remain reproducible even if the project advances later.

## Recipe language design objective

Do not design a general-purpose programming language unless the test domain proves it necessary.

The initial language study should try to express the canonical tests below with a small set of concepts such as:

- configure;
- act/set;
- wait;
- expect;
- negative expectation / never;
- flash/load asset;
- repeat;
- conditional flow;
- simultaneous/parallel expectations;
- parameters;
- operator prompt;
- capture;
- pass/fail.

Features such as arbitrary user-defined functions, unrestricted mutable global state, imports, threads and exception systems should not be added merely because they are common in programming languages.

The canonical tests are the requirements input for the DSL/editor decision.

## Canonical recipe tests

These tests are intentionally chosen to stress different language features. They are examples for language design, not a frozen Bison production test suite.

### 1. Basic power-up / smoke test

Purpose:

- establish DUT power configuration;
- energize the DUT;
- ensure current remains within limits;
- observe a simple boot indication.

Representative flow:

```text
Configure DUT power = 12 V, current limit = 3 A
Power ON
Expect current < configured limit
Wait for BOOT_OK = HIGH within 2 s
PASS
```

Language pressure:

- configuration;
- action;
- timeout;
- scalar assertion;
- pass/fail propagation.

### 2. Firmware flash + boot verification

Purpose:

- program a DUT;
- verify that the programmed image boots and produces an observable result.

Representative flow:

```text
Prepare DUT for programming
Flash selected firmware asset
Reset / power-cycle DUT
Expect BOOT_OK or protocol heartbeat within timeout
```

Language pressure:

- external assets/templates;
- parameter substitution;
- programmer operations;
- ordered actions;
- failure propagation.

### 3. Cause -> effect timing test

Purpose:

- verify that a stimulus causes an expected response within a defined timing window.

Representative flow:

```text
Set IGNITION = HIGH
Expect CAN heartbeat within 500 ms
Ensure FAULT never asserts during that window
```

Language pressure:

- event causality;
- temporal relationships;
- positive expectation;
- negative expectation;
- simultaneous observation.

### 4. Analog transfer-function test

Purpose:

- apply a range of analog stimuli and verify corresponding responses.

Representative flow:

```text
For each stimulus value:
    Set analog output
    Wait for settling
    Measure feedback
    Expect feedback within tolerance
Record measured value
```

Example points:

```text
0.5 V -> expect 0.45-0.55 V
1.0 V -> expect 0.95-1.05 V
...
```

Language pressure:

- loops;
- variables/iteration values;
- physical units;
- ranges/tolerances;
- measurement capture;
- result aggregation.

### 5. Fault injection / recovery test

Purpose:

- deliberately create an abnormal condition and verify DUT response and recovery.

Representative flow:

```text
Bring DUT to normal running state
Inject defined fault
Expect DUT fault indication / safe behavior
Remove injected fault
Reset or command recovery
Expect normal operation to return
```

Language pressure:

- explicit abnormal-state actions;
- state transitions;
- cleanup/recovery;
- assertions before and after fault removal.

### 6. Protocol transaction test

Purpose:

- send a protocol request and validate a structured response.

Representative flow:

```text
Configure CAN/UART/I2C/SPI interface
Transmit request
Expect response within timeout
Match identifier/fields/masks/payload as required
```

Language pressure:

- protocol configuration;
- structured message construction;
- structured matching;
- masks/field predicates;
- timeout.

### 7. Power sequencing test

Purpose:

- enforce order and timing between rails/signals.

Representative flow:

```text
Enable MAIN
Wait 20 ms
Enable AUX
Expect PGOOD within 100 ms
Assert RESET_N after required delay
```

Language pressure:

- precise timing;
- sequential actions;
- timing relative to prior events;
- possible simultaneous observations.

### 8. Repeated cycle / endurance test

Purpose:

- repeatedly exercise a DUT operation while retaining useful aggregate information.

Representative flow:

```text
Repeat N times:
    Power-cycle or issue command
    Verify expected response
    Capture current/timing measurements
Abort on failure, or continue according to recipe policy
Report min/max/timing/failure iteration
```

Language pressure:

- repeat counts;
- iteration identity;
- configurable abort/continue behavior;
- aggregation/statistics;
- large log volume.

### 9. Operator-assisted test

Purpose:

- include a controlled manual fixture/DUT step without reducing the recipe to an informal checklist.

Representative flow:

```text
Pause
Prompt: "Connect load to J3"
Require operator Continue
Resume automated test
```

Language pressure:

- operator prompt;
- pause/resume;
- explicit acknowledgement;
- audit trail of manual steps.

### 10. Parameterized production test

Purpose:

- run one recipe across units/revisions while resolving controlled run-specific inputs.

Representative parameters may include:

- serial number;
- board revision;
- firmware image;
- calibration constants;
- configured test limits.

Language pressure:

- typed parameters;
- defaults/required values;
- asset selection;
- resolved-run snapshot;
- traceability.

## Candidate minimum execution vocabulary

Before introducing syntax, the canonical tests suggest that the semantic core may be approximately:

```text
CONFIGURE
SET / ACT
WAIT
EXPECT
NEVER
FLASH / LOAD
REPEAT
IF
PARALLEL / simultaneous expectations
PARAMETER
PROMPT
CAPTURE
PASS / FAIL
```

This is a working semantic vocabulary, not a finalized language.

The next design activity is to express all canonical tests in the smallest practical pseudo-language and identify where the model becomes awkward. That exercise will determine whether Bison needs a purpose-built DSL, a structured data format such as YAML, direct scripting, or a hybrid representation.

## Local storage and controlled shutdown

Bison should include internal removable flash storage, with an internal SD or microSD card as the current preferred implementation.

The storage is part of the appliance, not a user-supplied workflow dependency. It exists to keep Bison operational and auditable when the host or network is unavailable.

### SD card responsibilities

The internal card should be sized and managed for:

- the currently loaded recipe and its required assets;
- run event logs;
- final structured run results;
- captures/waveforms requested by a recipe;
- pending CI/host synchronization;
- staged Bison runtime/update bundles;
- the current runtime bundle and, where practical, a previous known-good rollback bundle.

Capacity is not expected to be the limiting design constraint. The minimum economically sensible cards are already much larger than the ordinary Bison runtime and result data.

Retention should therefore be time-based rather than based on a small fixed run count.

Storage classes should behave as follows:

- active recipe/runtime: pinned;
- current run: pinned;
- completed but unsynchronized run: pinned;
- completed and synchronized run: eligible for expiry according to retention policy;
- staged candidate update: pinned until promoted or discarded;
- rollback runtime: pinned according to update policy.

Network loss must not invalidate a test that Bison can safely complete locally. Bison should continue execution, buffer results locally, and synchronize later while preserving original run identifiers and timestamps.

### Probable external SDRAM

The RA6M3 internal SRAM is sufficient for the control/runtime core but may be too small for high-rate capture buffering if direct streaming to SD or the network interferes with deterministic test execution.

The probable architecture is therefore:

```text
Internal SRAM
  - FreeRTOS/runtime
  - critical state
  - DMA descriptors
  - low-latency queues
        |
        v
External SDRAM
  - burst/sample buffers
  - protocol captures
  - waveform buffers
  - event/log coalescing
        |
        v
SD card
  - durable run storage
  - deferred synchronization
```

External SDRAM is not yet a frozen requirement or size. It should be included in implementation planning and resource allocation, with final capacity derived from worst-case aggregate capture bandwidth and the desired SD write granularity.

The intended use is buffering and coalescing, not placing safety-critical control state exclusively in external memory.

### Controlled shutdown on Bison power loss

Bison should include energy hold-up sufficient to detect loss of its own operating supply and perform a controlled shutdown.

The preferred direction is a dedicated hold-up capacitor/supercapacitor arrangement sized to keep the MCU and required storage path alive long enough to:

1. detect input-power loss;
2. stop accepting new work;
3. place DUT-facing outputs into the required safe state;
4. close or checkpoint the active run record;
5. flush critical buffered data and filesystem metadata to the SD card;
6. mark any interrupted run appropriately;
7. shut down cleanly before the hold-up rail collapses.

This does not replace the existing hardware fault/interlock paths. Immediate DUT safety remains hardware-controlled; the hold-up mechanism exists to preserve Bison state and storage integrity after its own input power is removed.

Implementation must determine:

- required hold-up time;
- capacitor/supercapacitor value and ESR;
- brownout/power-fail detection threshold;
- which internal rails remain powered during shutdown;
- SD-card worst-case flush/close timing;
- whether external SDRAM contents must be partially or fully drained;
- repeated power-cycle behavior;
- startup handling of an interrupted/incomplete run.

The controlled-shutdown budget should be derived from measured worst-case firmware and SD-card behavior rather than from nominal filesystem timings.


## Deferred decisions

The following are intentionally left open:

- recipe file syntax and extension;
- YAML versus a purpose-built DSL versus a scripting language;
- visual programming versus text-first editing versus both;
- exact online editor framework and hosting name;
- exact recipe schema;
- exact unique hostname convention for multi-Bison deployments;
- final USB-to-Ethernet accessory hardware/BOM;
- run-history retention limits on the instrument;
- Git authentication/workflow presented by the hosted editor;
- the division of parsing/execution work between browser and Bison firmware.

These decisions should be driven by the canonical tests and by implementation constraints rather than selected prematurely.
