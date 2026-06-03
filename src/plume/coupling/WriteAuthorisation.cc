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
#include "plume/coupling/WriteAuthorisation.h"

namespace plume {

const std::set<std::string> WriteAuthorisation::kEmptySet_{};

void WriteAuthorisation::grant(const std::string& pluginName, const std::string& paramName) {
    authorisations_[pluginName].insert(paramName);
}

bool WriteAuthorisation::isAuthorised(const std::string& pluginName, const std::string& paramName) const {
    auto it = authorisations_.find(pluginName);
    if (it == authorisations_.end()) {
        return false;
    }
    return it->second.count(paramName) > 0;
}

const std::set<std::string>& WriteAuthorisation::authorisedParams(const std::string& pluginName) const {
    auto it = authorisations_.find(pluginName);
    if (it == authorisations_.end()) {
        return kEmptySet_;
    }
    return it->second;
}

std::set<std::string> WriteAuthorisation::authorisedPlugins() const {
    std::set<std::string> result;
    for (const auto& [name, params] : authorisations_) {
        result.insert(result.end(), name);
    }
    return result;
}

}  // namespace plume
