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


    PluginHandler::PluginHandler(Plugin& plugin, const PluginConfig& config, const std::vector<std::string>& offeredParams) : 
    pluginRef_{plugin}, config_{config}, offeredParams_{offeredParams} {
}


void PluginHandler::activate(std::unique_ptr<PluginCore> plugincorePtr) {

    // plugincore ptr must not be null
    ASSERT(plugincorePtr);

    // the plugin is now active and associated to a plugincore
    plugincorePtr_ = std::move(plugincorePtr);
}


bool PluginHandler::isActive() const {    
    return (plugincorePtr_ != nullptr);
}

const std::vector<std::string>& PluginHandler::getRequiredParamNames() const {
    return offeredParams_;
}


void PluginHandler::grabData(const data::ModelData& data) {
    plugincorePtr_->grabData(data);
}


void PluginHandler::setup() {
    plugincorePtr_->setup();
}


void PluginHandler::run() {
    plugincorePtr_->run();
}


void PluginHandler::teardown() {
    plugincorePtr_->teardown();
}


} // namespace plume

