
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
#include "plume/Kernel.h"


namespace plume {

Kernel::Kernel(const eckit::Configuration& config) {}

Kernel::~Kernel() {}

std::string Kernel::name() const {
    return type();
}

// grab the data that it needs
void Kernel::grabData(const data::ModelData& data) {
    modelData_ = data;
};


data::ModelData& Kernel::modelData() {
    return modelData_;
}


// ---------------------------------------------------------
KernelFactory::KernelFactory() {}

KernelFactory::~KernelFactory() {}

KernelFactory& KernelFactory::instance() {
    static KernelFactory theinstance;
    return theinstance;
}

void KernelFactory::enregister(const std::string& name, const KernelBuilderBase& builder) {
    std::lock_guard<std::mutex> lock(mutex_);
    ASSERT(builders_.find(name) == builders_.end());
    builders_.emplace(std::make_pair(name, std::ref(builder)));
}

void KernelFactory::deregister(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = builders_.find(name);
    ASSERT(it != builders_.end());
    builders_.erase(it);
}

std::vector<std::string> KernelFactory::list_registered() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> reg_;
    for (const auto& b : builders_) {
        reg_.push_back(b.first);
    }
    return reg_;
}

Kernel* KernelFactory::build(const std::string& name, const eckit::Configuration& config) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = builders_.find(name);
    if (it == builders_.end()) {
        throw eckit::SeriousBug("Builder not found for backend " + name, Here());
    }

    return it->second.get().make(config);
}

KernelBuilderBase::KernelBuilderBase(const std::string& name) : name_(name) {
    KernelFactory::instance().enregister(name, *this);
}

KernelBuilderBase::~KernelBuilderBase() {
    KernelFactory::instance().deregister(name_);
}

}  // namespace plume