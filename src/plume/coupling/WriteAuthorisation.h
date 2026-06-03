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

#include <map>
#include <set>
#include <string>

namespace plume {

/**
 * @brief Records which plugins are authorised to write back which parameters, as determined during negotiation.
 *
 * Populated by the Negotiator after all plugins have been negotiated and stored by the Manager.
 * This class is used by Plume to enforce write-back policy at plugin run phase.
 */
class WriteAuthorisation {
private:
    /// pluginName -> set of parameter names the plugin is authorised to write.
    std::map<std::string, std::set<std::string>> authorisations_;

    static const std::set<std::string> kEmptySet_;

public:
    WriteAuthorisation() = default;

    /**
     * @brief Grant pluginName write access to paramName.
     *
     * Called internally by the Negotiator; not intended for direct use
     * by model or plugin code.
     */
    void grant(const std::string& pluginName, const std::string& paramName);

    /**
     * @brief Returns true if the plugin was granted write access to the named parameter.
     */
    bool isAuthorised(const std::string& pluginName, const std::string& paramName) const;

    /**
     * @brief Returns the set of parameter names the plugin may write.
     *
     * Returns a reference to an empty set if the plugin has no write authorisations.
     */
    const std::set<std::string>& authorisedParams(const std::string& pluginName) const;

    /**
     * @brief Returns all plugin names that have at least one write authorisation.
     */
    std::set<std::string> authorisedPlugins() const;

    bool empty() const { return authorisations_.empty(); }
};

}  // namespace plume
