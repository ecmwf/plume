Plume
=====

Description
-----------

Plume stands for PLUgin MEchanism and allows Earth Science applications to load plugins dynamically. Plume offers both an
API for controlling the loading mechanism and also transferring data from the calling application to the plugins.
A plugin can then read the input data transferred from the calling application and implement specific calculations
and analysis, providing a high-level of modularity and flexibility.

Architecture
------------

Plume features 3 major components:

 * Plugin Manager: that controls the loading mechanism at runtime
 * Plugin Data: Data transferred from the application to the plugin
 * Plugin: a Dynamically loadable plugin that implement specific calculations/algorithms

Plume offers API to this mechanism, available in multiple languages (currently C, C++ and Fortran)

License
-------
*Plume* is available under the open source `Apache License`__. In applying this licence, ECMWF does not waive
the privileges and immunities granted to it by virtue of its status as an intergovernmental organisation nor
does it submit to any jurisdiction.

__ http://www.apache.org/licenses/LICENSE-2.0.html

:Authors:
    Antonino Bonanni, James Hawkes, Tiago Quintino
:Version: 0.1.0