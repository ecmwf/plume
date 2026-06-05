Installation
============

Prerequisites
-------------

* CMake
* pybind11
* Python with development headers
* `eckit <https://github.com/ecmwf/eckit>`_
* `eccodes <https://github.com/ecmwf/eccodes>`_
* `atlas <https://github.com/ecmwf/atlas>`_
* Plume built with ``PLUME_ENABLE_NWP_EMULATOR=ON``
* `findlibs <https://github.com/ecmwf/findlibs>`_ (``pip install findlibs``)

Build
-----

Configure Plume with the Python interface enabled:

.. code-block:: bash

    cmake -DPLUME_ENABLE_NWP_EMULATOR=ON \
          -DPLUME_ENABLE_PYTHON_NWP_EMULATOR_INTERFACE=ON \
          ..
    make -j$(nproc)

Usage without install
---------------------

After a successful build, add the staging directory to ``PYTHONPATH``:

.. code-block:: bash

    export PYTHONPATH=<build_dir>/pynwp_emulator-staging:$PYTHONPATH
    python -c "import pynwp_emulator; print('OK')"

Precision variants
------------------

The Python extension module is always built as ``nwp_emulator_bindings`` and is
imported as ``nwp_emulator_bindings.nwp_emulator_bindings``.

CMake links the extension against the best available emulator precision at
configure time (double preferred, single fallback).
