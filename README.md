# plume

:warning: This software is still under heavy development and not 
yet ready for operational use

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
 * Plugin Data: data transferred from the application to the plugin
 * Plugin: a dynamically-loadable plugin that implement specific calculations/algorithms

plume offers API to this mechanism, available in multiple languages (currently C, C++ and Fortran)

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
