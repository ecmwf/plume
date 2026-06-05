NWP Emulator Python Interface
==============================

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   installation
   api

Description
-----------

**pynwp_emulator** is the Python interface to the Plume NWP emulator core.  It
wraps the high-performance C++ emulator via `pybind11 <https://pybind11.readthedocs.io>`_
and exposes a Pythonic API for driving emulator runs, inspecting field overlays,
and integrating with Plume plugins.

The C++ extension (``nwp_emulator_bindings``) is a thin layer that maps the
underlying C++ API one-to-one.  All docstrings, type hints, and Pythonic sugar
live in this pure-Python package so that IDEs and Sphinx can consume them
without compiling C++.
