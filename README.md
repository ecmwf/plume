# Plume

<p align="center">
<a href="https://github.com/ecmwf/codex/raw/refs/heads/main/Project%20Maturity">
    <img src="https://github.com/ecmwf/codex/raw/refs/heads/main/Project%20Maturity/emerging_badge.svg" alt="Project Maturity">
  </a>
<a href="https://github.com/ecmwf/codex/raw/refs/heads/main/ESEE"> <img src="https://github.com/ecmwf/codex/raw/refs/heads/main/ESEE/foundation_badge.svg" alt="ESEE">
</p>

> \[!IMPORTANT\]
> This software is **Emerging** and subject to ECMWF's guidelines on [Software Maturity](https://github.com/ecmwf/codex/raw/refs/heads/main/Project%20Maturity).

## Description
plume (plugin mechanism) allows Earth System models to load plugins
dynamically to offer access to data during model runtime. plume offers APIs for
controlling the loading mechanism and accessing data from the model to the plugins.
Plugins can be used to implement specific calculations, data analysis or even segregated
models, that are executed in close proximity to the model, thus minimising
a-posteriori data movements.

## Architecture
plume features 3 major components:

 * Plugin Manager: that controls the loading mechanism at runtime
 * [Plugin Data](./src/plume/data/): data transferred from the application to the plugin
 * Plugin: a dynamically-loadable plugin that implement specific calculations/algorithms

plume offers API to this mechanism, available in multiple languages (currently C, C++ and Fortran)

### Hook points

A model can call plume from several points within a single time step. It registers those points by
name during negotiation (`offerHook()` on the protocol it already builds), and invokes one of them
with `Manager::run("<name>")`. A plugin says where it wants to run either in its `negotiate()`
(`requireHook()`) or through an optional `hooks:` list in its entry of the plume configuration,
which *replaces* what the plugin declared and so lets a deployment re-target a plugin without
recompiling it. The match is resolved during negotiation: a plugin asking for a hook point the
model did not register is rejected. During its run a plugin can ask which hook point it is being
called from, and after negotiation the model can ask which parameters a given hook point consumes
(`getActiveParamsAtHook()`, `isParamRequestedAtHook()`) so that it can refresh exactly that data.

plume defines an implicit `"default"` hook point that is always registered. A plugin that declares
no hook point is bound to it, and the argument-less `Manager::run()` targets it — so models and
plugins that know nothing about hook points behave exactly as before.

> **When adopting hook points, keep calling the argument-less `Manager::run()`** where your single
> run call used to be. A model that only invokes its own named hook points leaves every plugin
> that declares no hook point — that is, every plugin written before this feature — silently
> dormant. The hook point summary that plume logs at the end of negotiation shows which plugins
> ended up bound to which hook point.

See `examples/example3.{cc,F90}` with `examples/plume_config_hooks.yml` for a worked example.

### Requirements
Build dependencies:

- C/C++ compiler (C++17)
- Fortran 2008 compiler
- CMake >= 3.16 --- For use and installation see http://www.cmake.org/
- ecbuild >= 3.5 --- ECMWF library of CMake macros (https://github.com/ecmwf/ecbuild)

Runtime dependencies:
  - eckit >= 1.20.0 (https://github.com/ecmwf/eckit)
  - Atlas >= 0.32.0 (https://github.com/ecmwf/atlas)

Optional runtime dependencies:  
  - fckit >= 0.9.5 (https://github.com/ecmwf/fckit)

### Installation
Plume employs an out-of-source build/install based on CMake.
Make sure ecbuild is installed and the ecbuild executable script is found ( `which ecbuild` ).
Now proceed with installation as follows:

```bash
# Environment --- Edit as needed
srcdir=$(pwd)
builddir=build
installdir=$HOME/local  

# 1. Create the build directory:
mkdir $builddir
cd $builddir

# 2. Run CMake
ecbuild --prefix=$installdir -- \
  -Deckit_ROOT=<path/to/eckit/install> \
  -Dfckit_ROOT=<path/to/fckit/install> \
  -Datlas_ROOT=<path/to/atlas/install> $srcdir

# 3. Compile / Install
make -j10
make install
```

### Testing
To test plume installation:

```bash
cd $builddir
make test
```

### Example Plugins
Additional example plugins can be found in https://github.com/ecmwf/plume-examples

