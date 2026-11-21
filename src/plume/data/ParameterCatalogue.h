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

#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "eckit/config/LocalConfiguration.h"

#include "plume/Configurable.h"
#include "plume/data/FieldProvider.h"
#include "plume/data/ParameterType.h"

namespace plume {
namespace data {

/**
 * @class ParameterDefinition
 * @brief A high level representation of a parameter of the data for catalogue building purposes.
 */
class ParameterDefinition : public CheckedConfigurable {
private:
    /**
     * @name Parameter configuration schema
     * @brief See ParameterCatalogue details on allowed schemas and options.
     */
    ///@{
    std::string name_;
    plume::data::ParameterType dataType_;
    std::string available_;
    std::string comment_;
    std::string levtype_;
    std::string level_;
    ///@}

    std::string strategy_;                   ///< Optional, strategy name, deduced from above options.
    std::string sourceParam_;                ///< Optional, original param that this param derives from.
    std::vector<std::string> dependencies_;  ///< Optional, based on the strategy requirements.
    bool writable_ = false;                  ///< Whether plugins are permitted to write back to this parameter.

    /// Optional hook scope: the hook points this param is required/offered at. Empty means unscoped, i.e. all of
    /// them (all the hook points the plugin is bound to, or all the hook points the model registered).
    std::set<std::string> hooks_;

    /// Hook points the param is writable at, if writable_. Empty means all the hook points of its (effective) scope.
    std::set<std::string> writableHooks_;

    /// All params must have a set of options which can be referred to here to avoid hardcoding in the constructors.
    inline static const std::unordered_set<std::string> essentialKeys_ = {"name", "type"};
    /**
     * @brief Stores all possible optional keys, including keys that enable parameter derivation (e.g., "height").
     *
     * All params are allowed a set of options which can be referred to here to avoid hardcoding in the constructors.
     * "hooks" is not a strategy option: it never takes part in the strategy matching.
     */
    inline static const std::unordered_set<std::string> optionalKeys_ = []() {
        std::unordered_set<std::string> keys = {"available", "comment", "levtype", "level", "writable", "hooks"};
        for (const auto& key : field_provider::UpdateStrategyTraits<int>::allConfigArgs) {
            keys.insert(std::string(key));
        }
        return keys;
    }();

    eckit::LocalConfiguration params2config(const std::string& name, const std::string& type,
                                            const std::string& available = "", const std::string& comment = "",
                                            bool writable = false, const std::set<std::string>& hooks = {},
                                            const std::set<std::string>& writableHooks = {});

    // keep the "hooks" and "writable" keys of the underlying config in sync with hooks_, writable_ and writableHooks_
    void syncAccessConfig();

    // a scoped "writable everywhere" definition is turned into an explicit list of writable hook points, so that
    // widening the scope in a merge does not widen the writes too
    void pinWritableHooks();

public:
    /**
     * @brief Constructs a parameter definition from config, finding the strategy matching the options, if applicable.
     *
     * @throws eckit::BadValue if the optional "hooks" key is not a non-empty list of strings, or if the optional
     *         "writable" key is neither a boolean nor a non-empty list of hook points (within "hooks", if present)
     */
    ParameterDefinition(const eckit::Configuration& config);

    /**
     * @param writable      whether the param is writable
     * @param hooks         hook scope (empty: unscoped)
     * @param writableHooks hook points the param is writable at (empty: all the hook points of its scope); only
     *                      meaningful when writable is true
     */
    ParameterDefinition(const std::string& name, const std::string& type, const std::string& available = "",
                        const std::string& comment = "", bool writable = false,
                        const std::set<std::string>& hooks = {}, const std::set<std::string>& writableHooks = {});
    ParameterDefinition(const std::string& name, const ParameterType& type, const std::string& available = "",
                        const std::string& comment = "", bool writable = false,
                        const std::set<std::string>& hooks = {}, const std::set<std::string>& writableHooks = {});
    ParameterDefinition(const std::string& name, const ParameterType& type,
                        const std::unordered_map<std::string, std::string>& options,
                        const std::set<std::string>& hooks = {});

    ~ParameterDefinition() = default;

    bool operator==(const ParameterDefinition& other) { return (name() == other.name() && type() == other.type()); }
    bool operator<(const ParameterDefinition& other) const { return name() < other.name(); }

    const std::string& name() const;
    const ParameterType& type() const;
    const std::string& available() const;
    const std::string& comment() const;
    const std::string& levtype() const;
    const std::string& level() const;
    const std::string& strategy() const;
    const std::string& sourceParam() const;
    const std::vector<std::string>& dependencies() const;

    /**
     * @brief Whether the param is writable at (at least) one hook point
     */
    bool writable() const;

    /**
     * @brief Hook scope of the param (empty if unscoped, i.e. valid at all hook points)
     */
    const std::set<std::string>& hooks() const;

    /**
     * @brief Hook points the param is writable at. Empty means: at every hook point of its (effective) scope,
     *        provided writable() is true.
     */
    const std::set<std::string>& writableHooks() const;

    /**
     * @brief Access mode at a hook point that is part of the (effective) scope of the param: true if writable there,
     *        false if read-only.
     */
    bool isWritableAt(const std::string& hook) const;

    /**
     * @brief Combine another definition of the same param into this one (per-hook access).
     *
     * - scope: an unscoped definition (on either side) makes the result unscoped, otherwise the scopes are united;
     * - access: at each hook point, the param is writable if any of the two definitions is writable there
     *   ("writable wins", per hook point). An unscoped writable definition makes the result writable everywhere.
     *
     * @return true if an unscoped "writable everywhere" definition overrode a definition that asked for read-only
     *         access at specific hook points (worth a warning)
     */
    bool mergeAccess(const ParameterDefinition& other);
};


/**
 * @class ParameterCatalogue
 * @brief A catalogue of parameter definitions, describing the YAML schema supported for the parameters.
 *
 * The following YAML document represents the parameter catalogue exposed by the manager during the negotiation phase.
 * Each entry under `params` describes a parameter’s name, type, availability conditions, and human-readable
 * documentation. Optionally, the schema can contain a level and levtype if the parameter is not directly provided by
 * the model, but can be derived by Plume from the provided parameters.
 *
 * Example:
 * @code{.yaml}
 * params:
 *   - name: param-1
 *     type: INT
 *     available: always
 *     comment: Param 1
 *   - name: param-2
 *     type: FLOAT
 *     available: on-request
 *     comment: Param 2
 *   - name: param-3
 *     type: ATLAS_FIELD
 *     available: on-request
 *     comment: Param 3
 *     height: 100
 *   - name: param-4
 *     type: ATLAS_FIELD
 *     hooks: [hook-A, hook-B]
 *     writable: [hook-A]
 *
 * Fields:
 * - name      : Unique parameter identifier.
 * - type      : Parameter type (as defined by the ParameterType enum).
 * - available : Availability condition (e.g. `always`, `on-request`).
 * - comment   : Human-readable description of the parameter.
 * - height    : The height in meters (wind field only).
 * - hooks     : Optional, non-empty list of hook points the parameter is required/offered at. When absent, the
 *               parameter is required/offered at every hook point.
 * - writable  : Optional, either a boolean (writable at every hook point of its scope) or a non-empty list of hook
 *               points (within "hooks", if present) the parameter is writable at; it is read-only at the others.
 *               Derived parameters cannot be writable.
 */
class ParameterCatalogue {
private:
    std::vector<ParameterDefinition> parameters_;

    // internal keys
    constexpr static const char* paramsKey() { return "params"; }

public:
    ParameterCatalogue();

    ParameterCatalogue(const eckit::Configuration& config);

    ParameterCatalogue(const ParameterCatalogue& other);

    ~ParameterCatalogue() = default;

    ParameterCatalogue& operator=(const ParameterCatalogue& other);

    const ParameterDefinition& getParam(const std::string& key) const;

    ParameterCatalogue& insertParam(const ParameterDefinition& param);

    /**
     * @brief Merge another definition into the existing definition of the same param (see
     *        ParameterDefinition::mergeAccess)
     * @return true if the merge widened writes over a read-only definition (see ParameterDefinition::mergeAccess)
     * @throws eckit::BadValue if the param is not in the catalogue
     */
    bool mergeParam(const ParameterDefinition& param);

    const std::vector<ParameterDefinition>& getParams() const;

    std::set<std::string> getParamNames() const;

    eckit::LocalConfiguration getConfig() const;

    bool hasParam(const std::string& name) const;

    ParameterCatalogue filter(const std::unordered_set<std::string>& params) const;
};


}  // namespace data
}  // namespace plume