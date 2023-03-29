
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
#include "plume/PluginCore.h"


namespace plume {

PluginCore::PluginCore(const eckit::Configuration& config) {}

PluginCore::~PluginCore() {}

std::string PluginCore::name() const {
    return type();
}

// grab the data that it needs
void PluginCore::grabData(const data::ModelData& data) {
    modelData_ = data;
};


data::ModelData& PluginCore::modelData() {
    return modelData_;
}


// ---------------------------------------------------------
PluginCoreFactory::PluginCoreFactory() {}

PluginCoreFactory::~PluginCoreFactory() {}

PluginCoreFactory& PluginCoreFactory::instance() {
    static PluginCoreFactory theinstance;
    return theinstance;
}

void PluginCoreFactory::enregister(const std::string& name, const PluginCoreBuilderBase& builder) {
    std::lock_guard<std::mutex> lock(mutex_);
    ASSERT(builders_.find(name) == builders_.end());
    builders_.emplace(std::make_pair(name, std::ref(builder)));
}

void PluginCoreFactory::deregister(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = builders_.find(name);
    ASSERT(it != builders_.end());
    builders_.erase(it);
}

std::vector<std::string> PluginCoreFactory::list_registered() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> reg_;
    for (const auto& b : builders_) {
        reg_.push_back(b.first);
    }
    return reg_;
}

PluginCore* PluginCoreFactory::build(const std::string& name, const eckit::Configuration& config) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = builders_.find(name);
    if (it == builders_.end()) {
        throw eckit::SeriousBug("Builder not found for backend " + name, Here());
    }

    return it->second.get().make(config);
}

PluginCoreBuilderBase::PluginCoreBuilderBase(const std::string& name) : name_(name) {
    PluginCoreFactory::instance().enregister(name, *this);
}

PluginCoreBuilderBase::~PluginCoreBuilderBase() {
    PluginCoreFactory::instance().deregister(name_);
}

}  // namespace plume