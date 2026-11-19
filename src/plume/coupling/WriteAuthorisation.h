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
 * @brief Records which plugins are authorised to write back which parameters at which hook points, as determined
 *        during negotiation.
 *
 * Populated by the Negotiator after all plugins have been negotiated and stored by the Manager.
 * This class is used by Plume to enforce write-back policy at plugin run phase: a plugin may write a parameter
 * during Manager::run(hook) only if it was granted write access to it at that hook point.
 */
class WriteAuthorisation {
private:
    /// pluginName -> parameter name -> hook points the plugin is authorised to write the parameter at.
    std::map<std::string, std::map<std::string, std::set<std::string>>> authorisations_;

public:
    WriteAuthorisation() = default;

    /**
     * @brief Grant pluginName write access to paramName at a hook point.
     *
     * Called internally by the Negotiator; not intended for direct use
     * by model or plugin code.
     */
    void grant(const std::string& pluginName, const std::string& paramName, const std::string& hook);

    /**
     * @brief Grant pluginName write access to paramName at the default hook point (plume::DEFAULT_HOOK).
     */
    void grant(const std::string& pluginName, const std::string& paramName);

    /**
     * @brief Returns true if the plugin was granted write access to the named parameter at the hook point.
     */
    bool isAuthorised(const std::string& pluginName, const std::string& paramName, const std::string& hook) const;

    /**
     * @brief Returns true if the plugin was granted write access to the named parameter at any hook point.
     */
    bool isAuthorised(const std::string& pluginName, const std::string& paramName) const;

    /**
     * @brief Returns the set of parameter names the plugin may write (at any hook point).
     */
    std::set<std::string> authorisedParams(const std::string& pluginName) const;

    /**
     * @brief Returns the names of the parameters that at least one plugin may write at the hook point, i.e. the
     *        write-back slots to open when that hook point is run.
     */
    std::set<std::string> writableParamsAt(const std::string& hook) const;

    /**
     * @brief Returns all plugin names that have at least one write authorisation.
     */
    std::set<std::string> authorisedPlugins() const;

    bool empty() const { return authorisations_.empty(); }
};

}  // namespace plume
