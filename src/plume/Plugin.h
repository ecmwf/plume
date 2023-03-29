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

#include <string>
#include <vector>

#include "eckit/log/Log.h"
#include "eckit/system/Plugin.h"

#include "plume/Protocol.h"


namespace plume {

/**
 * @brief A Plume Plugin
 * 
 */
class Plugin : public eckit::system::Plugin {

public:

    /**
     * @brief Construct a new Plugin object
     * 
     * @param name 
     * @param libname 
     */
    Plugin(const std::string& name, const std::string& libname = "");

    /**
     * @brief Destroy the Plugin object
     * 
     */
    virtual ~Plugin() override;

    /**
     * @brief Negotiate its run according to a protocol
     * 
     */
    virtual Protocol negotiate(/* input parameters? */) = 0;

    /**
     * @brief called at plugin loading
     * 
     */
    virtual void init() override{};

    /**
     * @brief called at plugin unloading
     * 
     */
    virtual void finalise() override{};

    /**
     * @brief Plugin Version
     * 
     * @return std::string 
     */
    std::string version() const override { return ""; }

    /**
     * @brief Plugin Hash
     * 
     * @param count 
     * @return std::string 
     */
    std::string gitsha1(unsigned int count) const override { return "undefined"; }

    /**
     * @brief Name of the plugincore available in the plugin
     * 
     * @return std::string 
     */
    virtual std::string plugincoreName() const = 0;

};

}  // namespace plume