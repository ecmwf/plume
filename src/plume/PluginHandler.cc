/*
 * (C) Copyright 2023- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 *
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */
#include "plume/PluginHandler.h"

namespace plume {

PluginHandler::PluginHandler(Plugin* plugin) : pluginPtr_{plugin} {

}

PluginHandler::~PluginHandler() {

}

void PluginHandler::activate(plume::Kernel* kernelPtr) {

    // kernel ptr must not be null
    ASSERT(kernelPtr);

    // the plugin is not active and associated to a kernel
    kernelPtr_ = kernelPtr;
}

void PluginHandler::deactivate() {

    // disassociate from the kernel
    kernelPtr_ = nullptr;
}

bool PluginHandler::isActive() const {    
    return (kernelPtr_ != nullptr);
}

Kernel* PluginHandler::kernel() const {
    return kernelPtr_;
}

Plugin* PluginHandler::plugin() const {
    return pluginPtr_;
}

} // namespace plume

