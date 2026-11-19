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

#include "plume/Hook.h"

namespace plume {

void WriteAuthorisation::grant(const std::string& pluginName, const std::string& paramName,
                               const std::string& hook) {
    authorisations_[pluginName][paramName].insert(hook);
}

void WriteAuthorisation::grant(const std::string& pluginName, const std::string& paramName) {
    grant(pluginName, paramName, DEFAULT_HOOK);
}

bool WriteAuthorisation::isAuthorised(const std::string& pluginName, const std::string& paramName,
                                      const std::string& hook) const {
    auto it = authorisations_.find(pluginName);
    if (it == authorisations_.end()) {
        return false;
    }
    auto pit = it->second.find(paramName);
    return pit != it->second.end() && pit->second.count(hook) > 0;
}

bool WriteAuthorisation::isAuthorised(const std::string& pluginName, const std::string& paramName) const {
    auto it = authorisations_.find(pluginName);
    if (it == authorisations_.end()) {
        return false;
    }
    return it->second.count(paramName) > 0;
}

std::set<std::string> WriteAuthorisation::authorisedParams(const std::string& pluginName) const {
    std::set<std::string> result;
    auto it = authorisations_.find(pluginName);
    if (it != authorisations_.end()) {
        for (const auto& [param, hooks] : it->second) {
            result.insert(result.end(), param);
        }
    }
    return result;
}

std::set<std::string> WriteAuthorisation::writableParamsAt(const std::string& hook) const {
    std::set<std::string> result;
    for (const auto& [plugin, params] : authorisations_) {
        for (const auto& [param, hooks] : params) {
            if (hooks.count(hook) > 0) {
                result.insert(param);
            }
        }
    }
    return result;
}

std::set<std::string> WriteAuthorisation::authorisedPlugins() const {
    std::set<std::string> result;
    for (const auto& [name, params] : authorisations_) {
        result.insert(result.end(), name);
    }
    return result;
}

}  // namespace plume
