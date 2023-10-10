plume
=====

.. Warning:: This software is still under development and not yet ready for operational use.

Description
-----------

**plume** (**plu**\ gin **me**\ chanism) allows Earth System models to load plugins
dynamically to offer access to data during model runtime. **plume** offers APIs for
controlling the loading mechanism and accessing data from the model to the plugins.
Plugins can be used to implement specific calculations, data analysis or even segregated
models, that are executed in close proximity to the model, thus minimising
a-posteriori data movements.

Architecture
------------

**plume** features 3 major components:

 * Plugin Manager: that controls the loading mechanism at runtime
 * Plugin Data: data transferred from the application to the plugin
 * Plugin: a dynamically-loadable plugin that implement specific calculations/algorithms

plume offers API to this mechanism, available in multiple languages (currently C, C++ and Fortran)

License
-------
**plume** is available under the open source `Apache License Version 2`__. In applying this licence, ECMWF does not waive
the privileges and immunities granted to it by virtue of its status as an intergovernmental organisation nor
does it submit to any jurisdiction.

__ http://www.apache.org/licenses/LICENSE-2.0.html

:Authors:
    Antonino Bonanni, James Hawkes, Tiago Quintino
:Version: 0.2.0
