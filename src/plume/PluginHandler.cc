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


PluginHandler::PluginHandler(Plugin& plugin, const PluginConfig& config, const PluginDecision& decision) :
    pluginRef_{plugin}, config_{config}, decision_{decision} {}


void PluginHandler::activate(std::unique_ptr<PluginCore> plugincorePtr) {

    // plugincore ptr must not be null
    ASSERT(plugincorePtr);

    // the plugin is now active and associated to a plugincore
    plugincorePtr_ = std::move(plugincorePtr);
}


bool PluginHandler::isActive() const {
    return (plugincorePtr_ != nullptr);
}

const std::set<std::string> PluginHandler::getRequiredParamNames(bool derived) const {
    return decision_.offeredParamNames(derived);
}

const std::set<plume::data::ParameterDefinition>& PluginHandler::getRequiredParams() const {
    return decision_.offeredParams();
}


void PluginHandler::grabData(data::ModelDataView data) {
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
    // Drop the grabbed model data view now — during the model's finalise() while atlas is still alive —
    // rather than at program exit (see PluginCore::releaseData()). The empty PluginCore itself lingers in
    // the process-static PluginRegistry (only the test-only Manager::reset() erases it), but once its data
    // view is released it holds no atlas::Field, so that lingering is harmless.
    plugincorePtr_->releaseData();
}

std::string PluginHandler::pluginName() const {
    return pluginRef_.name();
}


}  // namespace plume
