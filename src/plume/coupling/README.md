# Plume Write-Back Policy Reference

Plume write-back (two-way coupling) feature allows plugins to modify model parameters and return
values to the model. Because this is a sensitive capability with implications for
model correctness and reproducibility, it is **disabled by default** and must be
explicitly enabled via the `write-back-policy` key in the manager configuration.

---

## Configuration

```yaml
write-back-policy: single-writer   # or: single-writer-strict | multi-writer | multi-writer-strict | disabled
plugins:
  - lib: my_plugin_lib
    name: MyPlugin
    parameters:
      -
        - name: TEMP
          type: FLOAT
          writable: true     # this plugin requests write access to TEMP
    core-config: {}
```

If `write-back-policy` is absent, the default is **`disabled`**: any plugin that
requests a writable parameter is rejected at negotiation time. This ensures that
existing model configurations that do not use write-back are unaffected. This can also be enforced from the model side for model-owned parameters, by providing all parameters in read-only mode.

---

## Interaction Types

Three types of cross-plugin interaction become relevant once write-back is enabled:

In all cases **run order is determined by the order plugins appear in the
manager config** (first listed = first run). Plume logs the run order so you can reason about what each plugin will observe at runtime.

---

## Policies

### `disabled` (default)

No write-back is permitted, all plugins are one-way coupled (read-only) with the model. Any plugin requesting a writable parameter is
rejected at negotiation before it can be activated.

---

### `single-writer`

At most **one writer** per parameter. A second writer is rejected.

A reader (read-only) or derived-reader of a written parameter is **allowed**;
the run order is logged so the user can reason about what value they will observe:

**Run-order semantics for case Write-Read:**

- Writer runs **before** reader → reader sees the **written value**.
- Reader runs **before** writer → reader sees the **original model value**.

**Run-order semantics for Write-DerivedRead:**

- Writer runs **before** derived-reader → derived param is created from the **written value**.
- Derived-reader runs **before** writer → derived param is created from the **original model value**.

---

### `single-writer-strict`

At most **one writer** per parameter. A second writer is rejected.

Readers and derived-readers of a written parameter are also **rejected**
(the writer has exclusive access to the parameter — no other plugin may consume it).

Use this when you need guaranteed isolation — once a writer is accepted, no
other plugin may consume the same parameter (as a base or derived read).

> **Order sensitivity:** rejection is applied to whichever plugin arrives second
> in the config.  If a reader is listed before a writer, it is the **writer**
> that is rejected.  Place the plugin you want to keep first in the `plugins`
> list to guarantee it survives strict-mode negotiation.

---

### `multi-writer`

**Multiple writers** per parameter are allowed. All writers are accepted;
a `WARNING` with the full run order is logged after negotiation.

Readers and derived-readers of a written parameter are **allowed**; run order
is logged.

Use this when multiple plugins legitimately need to modify the same parameter
(e.g. a pipeline of post-processing stages at different entry points within the same model step), and you accept last-write-wins
semantics.

---

### `multi-writer-strict`

**Multiple writers** per parameter are allowed.

Readers and derived-readers of any written parameter are **rejected**
(writers collectively have exclusive use of the parameter).

> **Order sensitivity:** the plugin listed second in the config is rejected when
> a write-read or write-derived-read conflict is detected.  If a reader is
> listed before a writer, it is the **writer** that gets rejected.

---

## Policy Summary Table

| Policy | Write-Write | Write-Read | Write-DerivedRead |
|---|---|---|---|
| `disabled` | N/A (all writable requests rejected) | N/A | N/A |
| `single-writer` | 2nd writer **rejected** | Allowed, **run order logged** | Allowed, **run order logged** |
| `single-writer-strict` | 2nd writer **rejected** | 2nd plugin **rejected** | 2nd plugin **rejected** |
| `multi-writer` | All allowed, **run order logged** | Allowed, **run order logged** | Allowed, **run order logged** |
| `multi-writer-strict` | All allowed, **run order logged** | 2nd plugin **rejected** | 2nd plugin **rejected** |

"2nd plugin" refers to the plugin that **creates the conflict**, i.e. the later
entry in the manager config, regardless of whether that plugin is the writer or
the reader.

> **Order matters in strict mode.**  Rejection is always applied to the plugin
> that arrives second in the negotiation loop (= second in the `plugins` config
> list).  If the reader is listed before the writer, it is the **writer** that
> gets rejected, not the reader.  If you want a guaranteed outcome, place the
> plugin you want to keep first in the config.

---

## Run-Order Guarantee

Plugin activation order, and therefore run order, is **deterministic**: it
follows the order plugins appear in the `plugins` list of the manager config.

```yaml
plugins:
  - name: PluginA   # runs 1st
  - name: PluginB   # runs 2nd
  - name: PluginC   # runs 3rd
```

Plume logs the run order for every interaction involving a writable parameter so
the user can inspect the Plume log output and understand what each plugin will
observe.

---

## FAQ

**Why is `disabled` the default?**
Write-back is a sensitive feature that can affect model state. Requiring an
explicit opt-in via `write-back-policy` ensures that existing configurations are
not silently affected by plugins that happen to declare writable parameters.

**Can I use `writable: true` in the model's offers even under `disabled`?**
Yes. The model is free to declare which parameters *could* be writable in its
protocol offers. The `write-back-policy` only controls whether plugins are
*permitted* to request write access at negotiation time. A writable offer under
`disabled` will never result in a plugin being granted write access.

**What happens to a derived parameter when its source is modified?**
Derived parameters (e.g. `u;hl;100` derived from `u`) are computed from the
source parameter's value at the time the derived param is first requested during
`feedPlugins`. If a writer modifies the source *after* a derived-reader has
already set up its derived param, the derived-reader will see the original value.
The `Write-DerivedRead` warnings tell you exactly this ordering.

**Can derived parameters themselves be writable?**
No. Derived parameters update their value through an _update strategy_ (e.g.
interpolating from a base field to a specific level/height). Because the strategy
is the sole source of truth for the derived value, write-back to a derived
parameter would be immediately overwritten on the next strategy update, making it
meaningless. Plume therefore enforces this constraint at two levels:

1. **YAML config path** — `ParameterCatalogue` throws `eckit::BadParameter` at
   construction if a parameter definition carries both strategy options
   (`height`, etc.) and `writable: true`.

2. **C++ API path** — `Protocol::requireWritable<T>(name)` accepts only a bare
   parameter name and has no overload that also accepts strategy options. It is
   therefore impossible to construct a derived+writable `ParameterDefinition`
   through the plugin API. Passing a derived-looking name (e.g. `"u;hl;100"`)
   to `requireWritable` produces a non-derived definition, which is then
   rejected at negotiation because the model never offers `"u;hl;100"` as a
   base parameter.

Only base (non-derived) parameters support write-back.

## Runtime: How Write-Back Works

Write-back has two phases: **negotiation** (covered by `WriteAuthorisation`) and
**run phase** (covered by `WriteBackLedger` and the state machine described here).

### State machine per parameter slot

Each writable parameter has a slot in the `WriteBackLedger` that progresses
through the following states during each `run()` cycle:

```
IDLE ──open()──▶ READY ──writeParam()──▶ STAGED ──flush()──▶ FLUSHED ──acknowledgeWriteback()──▶ CONFIRMED
 ▲                │                                                                                    │
 │                │ (plugin chose not to write)                                                        │
 │                └────── flush() → IDLE (silent skip; param absent from pendingWritebacks())          │
 └──────────────────────────────────── reset() ────────────────────────────────────────────────────────┘
```

Any invalid transition (e.g. a second `writeParam` under `single-writer`, or a
`writeParam` outside a `run()` cycle) throws immediately with a precise message
identifying the parameter, the current state, and the attempted operation.

**State meanings:**

| State | Meaning |
|---|---|
| `IDLE` | Slot is inactive — between cycles, or never written |
| `READY` | Manager has opened the slot; authorised plugin may write this cycle |
| `STAGED` | A plugin has written a value; the write is applied immediately to model memory |
| `FLUSHED` | Write cycle closed; the model is being asked to acknowledge |
| `CONFIRMED` | Model has confirmed it has ingested the value (e.g. copy back the Atlas field)|
| `ERROR` | An unrecoverable write error occurred; slot is poisoned for this cycle |

### Error handling: poisoned slots

When a write fails (e.g. a type or Atlas-shape mismatch), the slot transitions to
`ERROR` via `reportError()`. Two things happen immediately, before `flush()` runs:

- **Write-back is disabled on the slot** — `reportError()` calls `disableWriteback()`
  right away, so a model-provided (raw-pointer) parameter cannot be mutated again this
  cycle.
- **Further writes are rejected** — `Error::onStage()` throws. Any subsequent
  `writeParam()` on a poisoned slot is refused at staging time, before the value can
  reach model memory. This is the guard that also protects Plume-owned parameters,
  whose storage is otherwise always mutable internally.

Together these enforce the rule that **a poisoned slot never receives further updates**.
All other triggers on an `ERROR` slot (`onOpen`/`onFlush`/`onAcknowledge`/`onReset`/`onError`)
are absorbed silently rather than throwing — they are driven by bulk Manager operations
that iterate every slot and must not break on a single poisoned one. `flush()` still
checks `hasErrors()` after transitioning all slots and throws, stopping execution.
Recovery is only possible via an explicit `clearError()` (`ERROR → IDLE`), which also
keeps write-back disabled.


### Flyweight state objects

State-specific behaviour is encapsulated in six concrete `WritebackState`
subclasses (`Idle`, `Ready`, `Staged`, `Flushed`,
`Confirmed`, `Error`). The `WriteBackLedger` owns **one instance of each**,
shared across all parameter slots within that ledger:

- Each `ParamSlot` holds a raw (non-owning) `const WritebackState*` pointer.
- A transition is a pointer swap: `slot.state = slot.state->onStage(...)`.
- **Zero per-slot heap allocation**, zero per-transition allocation.
- Adding a new state requires only a new subclass; no existing transition logic changes.

### Writes are applied immediately — no buffering

All writes go directly to the parameter's underlying storage when `writeParam()`
is called. There is no copy of the value inside the ledger. `flush()` performs
no data movement — it only advances the slot state from `STAGED` to `FLUSHED` to
trigger the model handshake.

This avoids a potentially expensive deep copy for Atlas fields (gigabytes of
field data) and has no correctness cost: the model only reads parameters after
`run()` returns, so there is no window where it could observe an intermediate
state.

However, Atlas fields are currently copies of the Fortran arrays. Therefore, the distinction between flush and confirmed is important to ensure the model has ingested the new field values.

### Plugin identity via `consumer_`

Each plugin receives a **filtered view** of `ModelData` (a copy scoped to its
required parameters). When the Manager creates this view it stamps a `consumer_`
field on it with the plugin's config name:

```cpp
// Manager::feedPlugins()
data.filter(requiredParams, pluginHandler.pluginName())
```

`writeParam` reads `consumer_` internally to identify the writing plugin for
authorisation checks and the audit log. Plugin code (C++, C, or Fortran) never
passes a plugin name explicitly — the identity is carried by the handle.

### Manager lifecycle

```
feedPlugins()        Creates WriteBackLedger, enrolls writable params, attaches ledger
                     to ModelData. Creates filtered views with consumer_ set per plugin.

run()                1. ledger.reset()  — CONFIRMED → IDLE (no-op on first call)
  [per cycle]        2. ledger.open()   — IDLE → READY for each authorised slot
                     3. plugins run     — each may call writeParam() → STAGED
                     4. ledger.flush()  — STAGED → FLUSHED (state only, no data movement)

                     Model calls pendingWritebacks() → ["PARAM_A", ...]
                     Model ingests values, then calls acknowledgeWriteback() per param → CONFIRMED

teardown()           Warns if any slot is still FLUSHED (unconfirmed). Detaches and destroys ledger.
```

### Multi-writer sequential composition

Under `multi-writer` policy, multiple plugins may write the same parameter in
a single cycle. Each write is applied immediately in plugin execution order;
each subsequent plugin reads the value already modified by its predecessors.
The ledger records the ordered list of writing plugins per slot for audit.

---
