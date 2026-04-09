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
#include <vector>

#include "eckit/config/Configuration.h"
#include "plume/data/ParameterCatalogue.h"
#include "plume/data/ParameterType.h"


namespace plume {

/**
 * @brief Used to match plugin requests and model offers
 */
class Protocol {
private:
    // internal utility functions
    void insertParam(const data::ParameterDefinition& param, data::ParameterCatalogue& catalogue);

    void setParamsFromConfig(const eckit::Configuration& config);

    std::string requestedPlumeVersion_;
    std::string requestedAtlasVersion_;
    data::ParameterCatalogue requiredParams_;

    std::string offeredPlumeVersion_;
    std::string offeredAtlasVersion_;
    data::ParameterCatalogue offeredParams_;

public:
    /**
     * @brief Construct a new Protocol object
     *
     */
    Protocol();

    /**
     * @brief Construct a new Protocol object
     *
     * @param config
     */
    Protocol(const eckit::Configuration& config);

    /**
     * @brief Destroy the Protocol object
     *
     */
    ~Protocol();


    // ------- required
    void requirePlumeVersion(const std::string& version);
    void requireAtlasVersion(const std::string& version);

    template <typename T>
    void require(const std::string& name) {
        insertParam(data::ParameterDefinition(name, data::deduceType<T>()), requiredParams_);
    }

    /**
     * @brief Lets plugins require params derived from model parameters in their negotiate method.
     */
    template <typename T>
    void require(const std::string& name, const std::unordered_map<std::string, std::string>& options) {
        insertParam(data::ParameterDefinition(name, data::deduceType<T>(), options), requiredParams_);
    }

    std::set<std::string> requiredParamNames() const;
    const std::string& requiredPlumeVersion() const;
    const std::string& requiredAtlasVersion() const;

    bool isParamRequired(const std::string& name) const;
    const data::ParameterCatalogue& requires() const;


    // ------- offered
    void offerPlumeVersion(const std::string& version);
    void offerAtlasVersion(const std::string& version);

    template <typename T>
    void offer(const std::string& name, const std::string& avail, const std::string& comment) {
        insertParam(data::ParameterDefinition(name, data::deduceType<T>(), avail, comment), offeredParams_);
    }

    std::set<std::string> offeredParamNames() const;
    const std::string& offeredPlumeVersion() const;
    const std::string& offeredAtlasVersion() const;

    bool isParamOffered(const std::string& name) const;
    const data::ParameterCatalogue& offers() const;
};


}  // namespace plume