# (C) Copyright 2025- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.

"""Pure-Python wrapper module for the NWP emulator native extension.

All symbols defined here delegate to the native C++ extension exposed via
``pynwp_emulator._internal``. This module serves three purposes:

1. **Documentation**: docstrings and type annotations are defined here in pure
   Python so that IDEs and Sphinx can consume them without compiling the C++
   extension.

2. **Auto-completion**: editors that refuse to introspect native extensions for
   security reasons will resolve symbols from this module instead.

3. **API extension point**: protocol methods such as context managers
   (``__enter__``/``__exit__``) and iterators (``__iter__``/``__next__``) that
   have no direct C++ equivalent are implemented here rather than in the
   binding layer.
"""

from __future__ import annotations

from pynwp_emulator._internal import (
    DataSourceType,
    FieldOverlaySnapshot,
    NWPEmulatorCore,
    RunOptions,
    RunResult,
)

__all__ = [
    "DataSourceType",
    "FieldOverlaySnapshot",
    "NWPEmulator",
    "RunOptions",
    "RunResult",
    "execute",
]


class NWPEmulator:
    """High-level Python interface to the NWP emulator core.

    Wraps :class:`NWPEmulatorCore` (the native C++ binding) and adds the
    context manager and iterator protocols so the step loop integrates
    naturally with Python control flow.

    If *options* are passed to the constructor, entering the context manager
    (``__enter__``) calls :meth:`setup_data_provider` and, when a Plume
    configuration is provided, :meth:`setup_plume_provider`. Exiting the
    context manager (``__exit__``) calls :meth:`finalize_run` (which also
    tears down Plume) unconditionally, even when an exception is raised inside
    the block.

    Example — context manager with iterator::

        import pynwp_emulator as emu

        opts = emu.RunOptions()
        opts.data_source_type = emu.DataSourceType.GRIB
        opts.data_source_path = "/path/to/data"
        opts.plume_config_path = "/path/to/plume.yml"

        with emu.NWPEmulator(opts) as emulator:
            for step in emulator:
                snapshot = emulator.get_field_overlay_snapshot("u,ml,1")

    Example — single-shot run::

        result = emu.NWPEmulator().execute(opts)

    Example — manual lifecycle (no context manager)::

        emulator = emu.NWPEmulator()
        emulator.setup_data_provider(opts)
        emulator.setup_plume_provider()
        while emulator.run_step():
            snapshot = emulator.get_field_overlay_snapshot("u,ml,1")
        emulator.finalize_run()
    """

    def __init__(self, options: RunOptions | None = None) -> None:
        """
        Parameters
        ----------
        options:
            Optional run options. When provided and the instance is used as a
            context manager, setup is performed automatically in
            ``__enter__``.
        """
        self._core = NWPEmulatorCore()
        self._options = options

    # ------------------------------------------------------------------
    # Context manager protocol
    # ------------------------------------------------------------------

    def __enter__(self) -> NWPEmulator:
        """Set up the emulator and return *self*.

        If *options* were supplied to the constructor, calls
        :meth:`setup_data_provider` and, when ``plume_config_path`` is
        non-empty, :meth:`setup_plume_provider` before returning.
        When ``plume_config_path`` is empty the emulator runs in dry-run
        mode: the data provider is initialised but Plume is not.
        """
        if self._options is not None:
            if not self.setup_data_provider(self._options):
                raise ValueError("Invalid run options: data_source_type and data_source_path must be set")
            if self._options.plume_config_path and not self.setup_plume_provider():
                raise RuntimeError("Failed to set up Plume provider")
        return self

    def __exit__(self, exc_type: object, exc_val: object, exc_tb: object) -> bool:
        """Tear down the emulator, regardless of whether an exception occurred.

        Calls :meth:`finalize_run`, which tears down Plume and waits for all
        MPI ranks to finish. Exceptions raised inside the ``with`` block are
        not suppressed.
        """
        self.finalize_run()
        return False  # do not suppress exceptions

    # ------------------------------------------------------------------
    # Iterator protocol
    # ------------------------------------------------------------------

    def __iter__(self) -> NWPEmulator:
        """Return *self* as the iterator (the emulator drives its own loop)."""
        return self

    def __next__(self) -> int:
        """Advance one model step and return the current step index.

        Raises :exc:`StopIteration` when the data source is exhausted.
        """
        if not self.run_step():
            raise StopIteration
        return self.current_step()

    def validate_run_options(self, options: RunOptions) -> bool:
        """Return ``True`` if *options* are complete and consistent.

        Parameters
        ----------
        options:
            Populated :class:`RunOptions` instance to validate.
        """
        return self._core.validate_run_options(options)

    def setup_data_provider(self, options: RunOptions) -> bool:
        """Initialise the data provider from *options*.

        Parameters
        ----------
        options:
            Must have ``data_source_type`` and ``data_source_path`` set.

        Returns
        -------
        bool
            ``True`` on success.
        """
        return self._core.setup_data_provider(options)

    def setup_plume_provider(self) -> bool:
        """Negotiate Plume field registration after data-provider setup.

        Returns
        -------
        bool
            ``True`` when Plume negotiation completed (or was skipped because
            ``plume_config_path`` was empty).
        """
        return self._core.setup_plume_provider()

    def run_step(self) -> bool:
        """Advance the emulator by one model step.

        Returns
        -------
        bool
            ``True`` while there are more steps to process, ``False`` once the
            data source is exhausted.
        """
        return self._core.run_step()

    def finalize_plume(self) -> None:
        """Tear down Plume after the step loop completes."""
        self._core.finalize_plume()

    def finalize_run(self) -> None:
        """Release all resources held by the emulator."""
        self._core.finalize_run()

    def current_step(self) -> int:
        """Return the index of the last completed model step."""
        return self._core.current_step()

    def available_field_keys(self) -> list[str]:
        """Return all field keys exposed by the data provider.

        Keys use the ``shortName,levtype,level`` format, e.g. ``"u,ml,1"``.
        """
        return self._core.available_field_keys()

    def get_field_overlay_snapshot(self, field_key: str) -> FieldOverlaySnapshot:
        """Return the rank-local field overlay for *field_key* at the current step.

        Parameters
        ----------
        field_key:
            A key returned by :meth:`available_field_keys`.

        Returns
        -------
        FieldOverlaySnapshot
            Contains ``lon``, ``lat``, ``values`` arrays for this rank, plus
            MPI topology metadata (``rank``, ``root``, ``nprocs``).
        """
        return self._core.get_field_overlay_snapshot(field_key)

    def execute(self, options: RunOptions) -> RunResult:
        """Run the full emulator lifecycle (setup → steps → teardown).

        Parameters
        ----------
        options:
            Fully populated run options.

        Returns
        -------
        RunResult
            Summary of the completed run.
        """
        return self._core.execute(options)


def execute(options: RunOptions) -> RunResult:
    """Convenience function: create a :class:`NWPEmulator` and call :meth:`~NWPEmulator.execute`.

    Parameters
    ----------
    options:
        Fully populated run options.

    Returns
    -------
    RunResult
        Summary of the completed run.
    """
    return NWPEmulator().execute(options)
