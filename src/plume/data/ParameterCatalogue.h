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

    /// All params must have a set of options which can be referred to here to avoid hardcoding in the constructors.
    inline static const std::unordered_set<std::string> essentialKeys_ = {"name", "type"};
    /**
     * @brief Stores all possible optional keys, including keys that enable parameter derivation (e.g., "height").
     *
     * All params are allowed a set of options which can be referred to here to avoid hardcoding in the constructors.
     */
    inline static const std::unordered_set<std::string> optionalKeys_ = []() {
        std::unordered_set<std::string> keys = {"available", "comment", "levtype", "level"};
        for (const auto& key : field_provider::UpdateStrategyTraits<int>::allConfigArgs) {
            keys.insert(std::string(key));
        }
        return keys;
    }();

    eckit::LocalConfiguration params2config(const std::string& name, const std::string& type,
                                            const std::string& available = "", const std::string& comment = "");

public:
    /** 
     * @brief Constructs a parameter definition from config, finding the strategy matching the options, if applicable.
     */
    ParameterDefinition(const eckit::Configuration& config);
    ParameterDefinition(const std::string& name, const std::string& type, const std::string& available = "",
                        const std::string& comment = "");
    ParameterDefinition(const std::string& name, const ParameterType& type, const std::string& available = "",
                        const std::string& comment = "");
    ParameterDefinition(const std::string& name, const ParameterType& type,
                        const std::unordered_map<std::string, std::string>& options);

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
 *
 * Fields:
 * - name      : Unique parameter identifier.
 * - type      : Parameter type (as defined by the ParameterType enum).
 * - available : Availability condition (e.g. `always`, `on-request`).
 * - comment   : Human-readable description of the parameter.
 * - height    : The height in meters (wind field only).
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

    const std::vector<ParameterDefinition>& getParams() const;

    std::set<std::string> getParamNames() const;

    eckit::LocalConfiguration getConfig() const;

    bool hasParam(const std::string& name) const;

    ParameterCatalogue filter(const std::unordered_set<std::string>& params) const;
};


}  // namespace data
}  // namespace plume