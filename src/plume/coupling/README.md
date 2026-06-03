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
