# Bison Lua recipe runtime and capability architecture

Date: **5 October 2026**

Status: **Architecture frozen for the recipe/runtime boundary. Implementation details may evolve without weakening these invariants.**

This record defines the Bison Lua execution model, capability boundary, hardware-configuration ownership, error/log reporting, host run workflow, and simulation contract.

It supersedes older Bison notes that left the recipe language open, proposed a browser recipe editor, or treated Lua as a candidate rather than the selected scripting surface.

## Core rule

> **Lua never owns hardware. Lua receives typed capabilities and issues requests to native services. Native services exclusively own hardware and may be replaced by behaviorally equivalent simulation backends.**

Recipe Lua is always treated as untrusted. A recipe may be buggy, malicious, or deliberately written by an adversary attempting to escape the sandbox, dump firmware, widen hardware access, corrupt state, or deny service.

The security objective is:

```text
Lua compromise
    != firmware compromise
    != hardware-configuration compromise
```

Creative use of approved capabilities is acceptable. Bison does not police whether an approved output is connected to a DUT, a fixture, a rack light, or something else. It enforces the hardware/resource contract.

## Runtime architecture

The reusable model is:

```text
Untrusted Lua application
        |
        v
typed Lua capability bindings
        |
        v
request / completion boundary
        |
        +---- hardware backend
        |
        +---- simulation backend
        |
        +---- test/mock backend
```

On embedded Bison:

```text
UNPRIVILEGED LUA TASK

Lua capability call
        |
        v
typed request
        |
        v
approved RTOS/privilege crossing
        |
---------------- MPU PRIVILEGE BOUNDARY ----------------
        |
        v
PRIVILEGED BISON SERVICE
        |
        +-- resolve capability
        +-- validate operation
        +-- validate arguments/state
        +-- perform hardware operation
        +-- return result/completion
```

A Lua binding must never call a hardware driver directly.

Bindings may only:

1. validate/convert Lua arguments;
2. refer to opaque capability handles;
3. submit typed service requests;
4. yield or wait for completion;
5. convert service failures into Lua errors.

Only privileged native services may invoke GPIO, SPI, CAN, ADC, power-control, or other hardware drivers.

## Capabilities, not pins

Physical resource identifiers never appear in the Lua ABI.

Lua may hold an opaque object such as:

```lua
local reset = bison.resources.reset
reset:set(true)
```

Internally that object may carry a handle such as:

```text
slot = 23
generation = 7
```

The number is not a GPIO number, peripheral index, or pin number. It indexes a privileged capability table.

Conceptually:

```text
Lua name
  RESET
    |
    v
opaque capability handle
  slot 23 / generation 7
    |
    v
privileged capability table
  GPIO_OUTPUT / PORT3.10 / SET permitted
    |
    v
native driver
```

A capability entry may contain:

- capability type;
- permitted operations;
- physical resource allocation;
- active configuration identity;
- generation/version;
- resource-state constraints;
- backend-specific implementation data.

Lua must not be able to construct a valid capability from a raw integer, discover physical pin identities, replace the userdata metatable, or reinterpret one capability type as another.

## Every operation is checked

Possession of a capability number is not authority by itself.

Every hardware-affecting request is checked by the privileged service:

```text
decode request
    |
    v
resolve capability handle
    |
    v
validate generation + active config
    |
    v
validate operation for capability type
    |
    v
validate arguments
    |
    v
validate current hardware/resource state
    |
    v
execute
```

For example, an injected request such as:

```text
capability = 29
operation  = GPIO_SET
value      = 1
```

must be rejected if capability 29 is an SPI capability.

Guessing IDs, replaying stale IDs, byte-stuffing the request transport, or deliberately emitting malformed operations must not widen access.

The authorization decision belongs to the privileged capability table, not to bytes supplied by Lua.

## Message model

Lua is an orchestration layer that produces requests and consumes completions.

A generic request is conceptually:

```text
request {
    capability
    operation
    arguments
}
```

and a completion:

```text
response {
    status
    result
}
```

For GPIO:

```text
capability = 23
operation  = SET
value      = true
```

For SPI:

```text
capability = 29
operation  = TRANSFER
tx         = [0x9F]
rx_length  = 3
```

The transport encoding is not part of the Lua API contract. It may be packed structs/bytes on embedded Bison and a different human-readable or test-oriented representation on a host simulator, provided the semantics remain identical.

For SPI and similar peripherals, the capability represents the approved logical interface/device contract. Lua does not receive SCK/MOSI/MISO/CS pins or a generic `spi[N]` escape hatch.

## Hardware configuration is outside Lua

Bison hardware/resource configuration lives outside recipe Lua.

A recipe project is expected to contain a privileged configuration artifact and untrusted behavior separately, for example:

```text
motor-bringup/
├── bison.yaml
├── bison.lock
├── bison.generated.lua
└── recipe.lua
```

The exact generated filenames may evolve, but the ownership split is fixed:

- **YAML/config** defines approved electrical and resource configuration;
- **generated Lua/LuaLS surface** exposes typed logical capabilities for authoring;
- **recipe Lua** expresses test/automation behavior only.

The configuration may be handwritten or generated by the browser.

Physical Bison accepts/activates new hardware configuration only through the privileged browser configuration flow, with explicit review and strong confirmation for dangerous changes. `bison run` is not allowed to alter electrical configuration.

The active configuration has a stable identity/hash. Generated scaffolding and lock metadata are derived from it. Drift or tampering must be detected before execution.

Generated Lua exists primarily for authoring ergonomics and LuaLS typing. Runtime hardware authority is injected from the validated native capability set; editing a generated Lua file must never create hardware authority.

## Make wrong Lua hard to write

The public Lua surface should expose logical, typed resources rather than generic mutable hardware objects.

Prefer:

```lua
reset:set(true)
boot_ok:read()
flash:transfer("\x9F", 3)
```

Do not expose:

```lua
gpio[10]
gpio.write(10, true)
resource:set_mode("output")
resource:set_function("spi")
resource.pin
resource.index
```

Input and output resources are distinct types. SPI, CAN, UART, ADC, power outputs, and other capabilities expose only operations valid for that capability class.

LuaLS annotations generated from the active configuration should catch obvious authoring mistakes before runtime. LuaLS is advisory developer tooling, not a security boundary.

Normal Lua language features remain useful. Restrict access to capabilities, not the ability to use loops, tables, functions, or coroutines.

For example:

```lua
---@type BisonGpioOutput[]
local outputs = {
    bison.resources.led1,
    bison.resources.led2,
    bison.resources.led3,
}

for _, output in ipairs(outputs) do
    output:set(false)
end
```

## Electrical/resource validation before execution

Bison must validate the resolved hardware plan before enabling drivers.

This is a resource/electrical safety check, not merely a no-hardware execution mode.

Validation includes, as applicable:

- output must not be bound against another driven output;
- two push-pull outputs must not share a net;
- open-drain sharing must be explicitly compatible;
- SPI directions and grouping must be valid;
- one physical channel cannot simultaneously serve conflicting functions;
- differential pairs must be valid;
- analog inputs must not be bound to driven digital outputs;
- power resources are separate from ordinary GPIO;
- mux conflicts are rejected;
- voltage-domain compatibility is enforced.

Only after the plan is valid may Bison arm DUT-facing hardware.

## Lua runtime isolation

The Lua task should run unprivileged under the RA6M3/Cortex-M4 privilege model. The MPU and related platform protections should isolate the Lua VM from:

- privileged firmware/native state;
- peripheral register space;
- capability database internals;
- boot/update paths;
- unrelated memory;
- protected DMA/bus-master regions where practical.

The Lua environment should be deliberately small.

Retain useful language functionality such as functions, tables, loops, conditionals, coroutines, and selected string/math/table helpers.

Do not expose unnecessary host capabilities such as:

- filesystem access;
- OS/process APIs;
- arbitrary network APIs;
- dynamic native module loading;
- unrestricted `require`;
- `dofile` / `loadfile`;
- unrestricted dynamic code generation;
- the normal recipe-visible `debug` library.

The runtime should use bounded memory, bounded coroutine count, bounded log/event queues, and execution/runtime budgets appropriate to the product.

Production debug/readout protection, firmware-update protection, and device-level hardening remain required because Lua sandboxing alone is not a complete physical-extraction defense.

## Errors and Lua execution failure

Lua does not use C++-style exceptions, but it has protected errors through `error`, `pcall`, `xpcall`, and the C API.

Invalid I/O operations are ordinary Lua execution errors, not Bison firmware panics.

For example:

```lua
reset:read()
```

on an output-only capability should ultimately surface as something equivalent to:

```text
BisonCapabilityError: READ is not permitted on RESET
```

The host must invoke/resume recipe execution through a protected boundary so an uncaught recipe error produces:

```text
Lua error
    |
    v
recipe/test execution terminates
    |
    v
structured failure event emitted
    |
    v
Bison firmware continues running
```

Useful error classes may include capability, I/O, timeout, and state failures, but the exact Lua representation remains an implementation detail.

Bison should construct useful source traceback information at the host/runtime boundary without exposing the full Lua `debug` library to recipe code.

A user-script error must never become a firmware panic.

## Print, logs, errors and run events

Lua has no direct stdout/stderr ownership on Bison.

The global `print(...)` exposed to recipes is a Bison binding that emits a bounded structured run event.

Conceptually:

```text
Lua
 ├── print() ------> LOG event
 ├── test API -----> TEST/RESULT events
 └── error --------> ERROR + traceback event
                         |
                         v
                  Bison run event stream
                         |
              +----------+----------+
              |                     |
              v                     v
           Web UI                bison run
```

Recipe print output is informational. Test pass/fail must never be inferred from printed text.

Log/event paths need hard limits on message size, rate, queue depth, and aggregate memory so hostile or accidental print floods cannot exhaust Bison.

## Test discovery

Bison should not parse Lua source text to discover tests.

Loading a recipe registers suites/tests through the recipe API. During discovery, registration builds an authoritative native registry while test bodies remain callbacks and are not executed.

The browser and CLI consume that registry to display/select tests.

Top-level recipe load/discovery must not permit hardware-affecting operations.

Stable machine IDs and separate display names are preferred where needed for reproducible selection.

## `bison run` workflow

Recipe authoring is local and Git-based. The browser recipe editor is not part of the architecture.

The normal developer flow is:

```text
local Lua project
    |
    v
bison run recipe.lua
    |
    +-- validate/package/hash
    +-- upload if required
    +-- request run
    +-- open WebSocket
    +-- stream structured run events
    +-- render terminal output
    +-- return meaningful exit status
```

The WebSocket carries structured events. The CLI renders them for terminals and automation.

Suggested stream mapping:

- recipe `print()`, progress and normal measurements/results -> stdout;
- Lua/runtime errors, transport errors, Bison faults and CLI diagnostics -> stderr.

The CLI should emit compiler-style source locations for errors:

```text
recipe.lua:42:17: error: READ is not permitted on RESET
recipe.lua:71:5: note: in function 'power_up'
recipe.lua:103:1: note: in test 'startup'
```

This enables editor integration without a proprietary plugin.

A VS Code problem matcher can consume the output with a pattern equivalent to:

```text
^(.+\.lua):(\d+):(\d+):\s+(error|warning|note):\s+(.*)$
```

A `launch.json`/task can invoke `bison run`, after which errors become clickable editor diagnostics. Other editors can integrate the same CLI contract through ordinary task/run configuration.

Terminal ANSI styling may be added when attached to a TTY, but the underlying text format must remain stable and machine-parseable.

## Simulation

Simulation is a backend replacement, not a second recipe system.

The same Lua request stream must work against:

- real Bison hardware services;
- a POSIX/simulation backend;
- test/mock backends.

The simulator may use a different message encoding internally, but it must preserve capability, operation, completion, error, and timing semantics visible to Lua.

This allows the same recipe to move from deterministic virtual-time simulation to physical Bison without API changes.

## Safe state

Recipe failure, timeout, VM failure, host disconnect, malformed requests, watchdog events, or deliberate abuse must not leave hardware in an unsafe intermediate state.

Safe-state convergence belongs below Lua in native/hardware-controlled mechanisms.

The exact safe state is product/config dependent, but Lua must not be required to execute cleanup code for Bison to become safe.

## Product boundary

Bison is intentionally programmable enough that users will apply it beyond narrowly defined DUT testing.

Examples may include lab automation, fixture control, protocol poking, power sequencing, manufacturing jigs, bench glue logic, or even frivolous uses such as rack lighting.

That is acceptable.

The product boundary is therefore:

> **Users may creatively use every capability they were explicitly granted. They may not turn that capability set into broader hardware or firmware authority.**

## Cross-product extraction

The generic runtime boundary should be extracted into a reusable APSL component, provisionally named **`apsl-device-lua`**.

Bison-specific concepts such as DB25 mapping, DUT power, Bison configuration UI, and Bison run/test semantics must remain outside that repository.

The reusable layer should focus on:

- Lua VM setup and restricted environment;
- bounded allocator/runtime support;
- protected execution and traceback capture;
- opaque capability userdata;
- typed request/completion abstraction;
- capability validation hooks;
- coroutine/yield integration;
- backend/transport abstraction;
- host/POSIX and embedded backends where appropriate;
- test/mock support;
- helper machinery for generated LuaLS-facing bindings.

The dependency direction should remain:

```text
apsl
   ^
apsl-device-lua
   ^
Bison / motor controller / future products
```

The next discussion should define the exact boundary and public API of `apsl-device-lua` without importing Bison product semantics into it.
