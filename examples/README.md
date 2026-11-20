# Plume examples

| Example | Sources | Configuration | What it shows |
|---|---|---|---|
| 1 | `example1.{c,cc,F90}` | `plume_config.yml` | Configure Plume, negotiate, share data and run the plugins |
| 2 | `example2.{cc,F90}` | `plume_config.yml` | Insert parameters into the data only if a plugin requested them |
| 3 | `example3.{cc,F90}` | `plume_config_hooks.yml`, `plume_config_hooks_params.yml` | Parameters created and updated by the model; first use of hook points (read-only parameters) |
| 4 | `example4.{cc,F90}`, `plugin_hooks.{h,cc}` | `plume_config_hooks_writeback.yml` | Hook points with per-hook access: parameters that are read-only at one hook point and writable at another |

All the examples are run by `run_plume_examples.sh`, generated in the `bin` directory of the build.

The rest of this document describes hook points and how they relate to read-only and writable
parameters, using example 4 as the reference. The write-back mechanism itself (policies, conflicts
between plugins, life cycle of a written parameter) is described in
[`src/plume/coupling/README.md`](../src/plume/coupling/README.md).

---

## 1. Hook points

A **hook point** is a named place of the model time step from which the model calls Plume. A model
that calls Plume from several places registers one hook point for each of them, and plugins choose
at which of them they run.

```
for each step:
    Manager::run("pre-physics")      <- plugins bound to "pre-physics" run here
    ... physics ...
    Manager::run("post-physics")     <- plugins bound to "post-physics" run here
```

### The model registers hook points

Hook points are registered on the protocol that the model already builds for negotiation:

```c++
plume::Protocol offers;
offers.offerHook("pre-physics", "before the physics is computed");
offers.offerHook("post-physics", "after the physics has been computed");
```

```fortran
call plume_check(offers%offer_hook("pre-physics", "before the physics is computed"))
call plume_check(offers%offer_hook("post-physics", "after the physics has been computed"))
```

### The `default` hook point

Plume always registers an implicit hook point called `default`:

- the argument-less `Manager::run()` runs it;
- a plugin that asks for no hook point is bound to it.

This is what keeps models and plugins that know nothing about hook points working as before. It
also means that **a model adopting hook points should keep calling the argument-less
`Manager::run()`** where its single run call used to be: otherwise every plugin that declares no
hook point is accepted but never run.

### Plugins are bound to hook points

A plugin asks for hook points in one of two ways:

| Where | How |
|---|---|
| In its `negotiate()` | `protocol.requireHook("pre-physics");` |
| In its entry of the Plume configuration | `hooks: [pre-physics, post-physics]` |

The rules are:

- The `hooks` list of the configuration **replaces** the hook points required in `negotiate()` (it
  is not added to them). A deployment can therefore move a plugin to another hook point without
  recompiling it.
- A plugin that asks for no hook point, in either place, is bound to `default`. To bind a plugin
  back to it from the configuration, write `hooks: [default]`; an empty list is an error.
- Every hook point a plugin asks for must be registered by the model. If one is missing, the
  plugin is **rejected** as a whole.

### Running a hook point

`Manager::run("name")` runs the plugins bound to that hook point, in the order in which they
appear in the configuration. Calling a hook point that the model did not register is an error.

A plugin bound to several hook points still has a single `run()` method, called once per
`Manager::run(hook)` call. It finds out where it is being run with `currentHook()`:

```c++
void PluginCoreHooks::run() {
    const std::string& hook = currentHook();
    if (hook == "pre-physics") { ... }
    else if (hook == "post-physics") { ... }
}
```

The order in which different hook points are called, and how many times, is decided by the model
only. Plume does not know it.

---

## 2. Parameters and hook points

Without hook points, a parameter is either shared with a plugin or not. With hook points, two more
things are decided **for each hook point**:

1. whether the parameter is used there at all (its **hook scope**);
2. whether it is **read-only** or **writable** there.

Both are declared on the two sides of the negotiation: by the model in what it offers, and by the
plugin in what it requests.

### Hook scope

| | No scope given | Scope given |
|---|---|---|
| **Model offers** | The parameter is offered at every hook point | It is offered at the listed hook points only |
| **Plugin requests** | The parameter is needed at every hook point the plugin is bound to | It is needed at the listed hook points only, which must be among the hook points the plugin is bound to |

A parameter must be offered at **every** hook point it is requested at, otherwise the plugin is
rejected. A parameter that is not offered at a hook point cannot be requested there, not even
read-only.

The scope is useful to the model as well: after negotiation it can ask which parameters each hook
point actually consumes, and refresh only those before calling it (see section 5).

### Access mode: read-only or writable

A parameter is read-only unless stated otherwise. `writable` can take three forms in the
configuration:

| `writable` | Meaning |
|---|---|
| absent or `false` | read-only at every hook point of the scope |
| `true` | writable at every hook point of the scope |
| `[hook, ...]` | writable at the listed hook points, read-only at the other hook points of the scope |

The hook points listed in `writable` must be inside the scope when a `hooks` key is given.

The equivalent calls from code are:

| | Read-only | Writable |
|---|---|---|
| **Model, every hook point** | `offer<T>(name, avail, comment)` | `offer<T>(name, avail, comment, true)` |
| **Model, listed hook points** | `offerAt<T>(name, avail, comment, {hooks})` | `offerAt<T>(name, avail, comment, {hooks}, true)` |
| **Plugin, every hook point it is bound to** | `require<T>(name)` | `requireWritable<T>(name)` |
| **Plugin, listed hook points** | `requireAt<T>(name, {hooks})` | `requireWritableAt<T>(name, {hooks})` |

In Fortran the model uses `offer_<type>(name, avail, comment [, hooks] [, writable])`, where
`hooks` is a comma-separated list of hook point names.

### Mixing access modes: several definitions of the same parameter

The same parameter can be offered or requested several times, with different scopes and access
modes. The definitions are **merged hook point by hook point**:

- the scopes are united (a definition without scope makes the result unscoped);
- at each hook point the parameter is writable if **any** of the definitions makes it writable
  there ("writable wins").

This is how a parameter gets a different access mode at different hook points. The three forms
below are equivalent, for a plugin bound to `pre-physics` and `post-physics`:

```yaml
# 1) one entry, list form of "writable"
- name: temperature
  type: double
  writable: [post-physics]
```

```yaml
# 2) one entry per hook point
- name: temperature
  type: double
  hooks: [pre-physics]
- name: temperature
  type: double
  hooks: [post-physics]
  writable: true
```

```c++
// 3) from the negotiate() of the plugin
protocol.requireAt<double>("temperature", {"pre-physics"});
protocol.requireWritableAt<double>("temperature", {"post-physics"});
```

Requests made from `negotiate()` and from the configuration are merged in the same way.

One case deserves attention: an **unscoped** `writable: true` merged with a definition that asks
for read-only access at specific hook points wins everywhere, and the parameter becomes writable
at every hook point of the plugin. Plume logs a warning when this happens.

### When is write access granted?

A plugin is granted write access to a parameter at a hook point when all of these hold:

1. the plugin is bound to that hook point, and the parameter is requested writable there;
2. the model offers the parameter **writable at that hook point**;
3. `write-back-policy` in the configuration is not `disabled` (it is by default);
4. the request does not conflict with another plugin, according to the policy (see below);
5. the parameter is not a derived parameter (those can never be written).

If one of the first three conditions fails, the plugin is rejected as a whole: Plume never
downgrades a writable request to read-only.

### Conflicts between plugins

With write-back, two plugins using the same parameter may conflict. The policy is applied between
requests **at the same hook point**:

| `write-back-policy` | write + write, same hook point | write + read, same hook point | different hook points |
|---|---|---|---|
| `single-writer` | 2nd plugin rejected | allowed, logged | allowed, logged |
| `single-writer-strict` | 2nd plugin rejected | 2nd plugin rejected | allowed, logged |
| `multi-writer` | allowed, logged | allowed, logged | allowed, logged |
| `multi-writer-strict` | allowed, logged | 2nd plugin rejected | allowed, logged |

Requests at different hook points never conflict: which one comes first depends on the order in
which the model calls the hook points, which Plume does not know. A plugin never conflicts with
itself, so it can write a parameter at one hook point and read it at another one.

---

## 3. What happens at run time

Each `Manager::run(hook)` call is one **write cycle**:

1. Plume opens for writing only the parameters that some plugin may write **at that hook point**.
   All the other parameters are closed, including the ones that are writable at other hook points.
2. The plugins bound to the hook point run. A plugin writing a parameter that is not open for it at
   this hook point gets an exception. A plugin can check beforehand with
   `modelData().isWritable(name)` (Fortran: `data%is_writable(name)`), which answers for the hook
   point being run.
3. When the call returns, the parameters that were actually written are **pending**.

The model then ingests the pending parameters and **acknowledges** each of them:

```c++
plume::Manager::run("pre-physics");
for (const auto& name : data.pendingWritebacks()) {
    // copy the new value back into the model, if needed
    data.acknowledgeWriteback(name);
}
```

```fortran
call plume_check(manager%run("pre-physics"))
pending = data%pending_writebacks()          ! comma-separated names
if (index(pending, "forcing") > 0) then
  call plume_check(data%acknowledge_writeback("forcing"))
end if
```

Two rules follow from this:

- **A pending parameter cannot be written again.** A `Manager::run(hook)` call that has to open a
  parameter which is still pending fails before any plugin runs. Parameters written at other hook
  points do not block it.
- **A pending parameter can still be read.** If a hook point that uses the parameter is run while
  it is pending, Plume only logs a warning: if the model refreshed the parameter in the meantime,
  the value written by the plugin has been overwritten.

The simplest way to satisfy both is to ingest and acknowledge right after each
`Manager::run(hook)` call, as the examples do.

---

## 4. Walkthrough of example 4

The model (`example4.cc`, `example4.F90`) registers `pre-physics` and `post-physics`, and shares
three parameters. The plugin (`plugin_hooks.{h,cc}`) declares nothing in its `negotiate()`:
everything it asks for is in `plume_config_hooks_writeback.yml`. Each parameter is used at both
hook points:

| Parameter | `pre-physics` | `post-physics` |
|---|---|---|
| `step` | read-only | read-only |
| `forcing` | **writable** | read-only |
| `temperature` | read-only | **writable** |

The model offers exactly this:

```c++
offers.offer<int>("step", "always", "current step of the model");

offers.offerAt<double>("forcing", "always", "...", {"pre-physics"}, /*writable=*/true);
offers.offerAt<double>("forcing", "always", "...", {"post-physics"});

offers.offerAt<double>("temperature", "always", "...", {"pre-physics"});
offers.offerAt<double>("temperature", "always", "...", {"post-physics"}, /*writable=*/true);
```

and the configuration requests it:

```yaml
write-back-policy: single-writer
plugins:
  - name: PluginHooks
    lib: plugin_hooks
    hooks: [pre-physics, post-physics]
    parameters:
      -
        - name: step
          type: int
          hooks: [pre-physics, post-physics]
        - name: forcing
          type: double
          hooks: [pre-physics, post-physics]
          writable: [pre-physics]
        - name: temperature
          type: double
          hooks: [pre-physics, post-physics]
          writable: [post-physics]
```

One time step then goes as follows:

| Model | Plugin | Written parameter |
|---|---|---|
| `run("pre-physics")` | reads `step` and `temperature`, writes `forcing` | `forcing` becomes pending |
| ingests and acknowledges `forcing` | | `forcing` acknowledged |
| physics: `temperature += forcing` | | |
| `run()` (default hook point) | no plugin bound to it in this example | |
| `run("post-physics")` | reads `step` and `forcing`, writes `temperature` | `temperature` becomes pending |
| ingests and acknowledges `temperature` | | `temperature` acknowledged |

Things to try, to see the rules of section 2 at work:

- Remove `write-back-policy` from the configuration: the plugin is rejected, because it requests
  writable parameters while write-back is disabled.
- Change `writable: [pre-physics]` of `forcing` to `writable: true`: the plugin is rejected,
  because the model does not offer `forcing` writable at `post-physics`.
- Add a hook point that the model does not register to the `hooks` list of the plugin: the plugin
  is rejected.
- Uncomment the write of `temperature` at `pre-physics` in `plugin_hooks.cc`: negotiation
  succeeds, and the write throws at run time because the parameter is not open at that hook point.

---

## 5. What the model can ask after negotiation

| C++ (`plume::Manager`) | Fortran (`plume_manager`) | Answer |
|---|---|---|
| `registeredHooks()` | `registered_hooks()` | Hook points registered by the model, including `default` |
| `isHookActive(hook)` | `is_hook_active(hook, flag)` | Is any plugin bound to this hook point? |
| `getActiveParamsAtHook(hook)` | `active_params_at_hook(hook)` | Parameters used by the plugins at this hook point: what to keep up to date before calling it |
| `isParamRequestedAtHook(name, hook)` | `is_param_requested_at_hook(name, hook, flag)` | Is this parameter used at this hook point? |
| `getWritableParamsAtHook(hook)` | `writable_params_at_hook(hook)` | Parameters that the plugins may write at this hook point: what may have to be ingested after calling it |

The Fortran functions that return lists return them as comma-separated strings.

Plume also logs a summary at the end of negotiation: which plugins are bound to each hook point,
and for each of them the parameters used there with their access mode.
