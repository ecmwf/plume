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
#pragma once

#include "plume/Plugin.h"
#include "plume/Kernel.h"


namespace plume {

/**
 * @brief Handles a Plugin (and its association to a Kernel)
 * 
 */
class PluginHandler {

public:

    PluginHandler(Plugin* plugin);

    ~PluginHandler();

    /**
     * @brief is Active
     * 
     * @return true 
     * @return false 
     */
    bool isActive() const ;

    /** Activate this plugin (associating it to a kernel)
     * @brief 
     * 
     * @param kernelPtr 
     */
    void activate(Kernel* kernelPtr);

    /**
     * @brief deactivate the plugin
     * 
     */
    void deactivate();

    /**
     * @brief Get the kernel pointer
     * 
     * @return plume::Kernel* 
     */
    plume::Kernel* kernel() const ;


    /**
     * @brief get the plugin pointer
     * 
     * @return plume::Plugin* 
     */
    plume::Plugin* plugin() const ;

private:

    // internal Plugin ptr
    Plugin* pluginPtr_;

    // internal Kernel ptr
    Kernel* kernelPtr_;
};

}  // namespace plume