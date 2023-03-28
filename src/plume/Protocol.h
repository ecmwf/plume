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

#include <string>
#include <vector>

#include "eckit/config/Configuration.h"
#include "plume/data/ParameterCatalogue.h"


namespace plume {

/**
 * @brief Used to match plugin requests and model offers
 */
class Protocol {

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
    void requireInt(const std::string& name);
    void requireBool(const std::string& name);
    void requireFloat(const std::string& name);
    void requireDouble(const std::string& name);
    void requireAtlasField(const std::string& name);

    std::vector<std::string> requiredParamNames() const;
    const std::string&       requiredPlumeVersion() const;
    const std::string&       requiredAtlasVersion() const;

    bool isParamRequired(const std::string& name) const;
    const data::ParameterCatalogue& requires() const;


    // ------- offered
    void offerPlumeVersion(const std::string& version);
    void offerAtlasVersion(const std::string& version);

    void offerInt(const std::string& name, const std::string& avail, const std::string& comment);
    void offerBool(const std::string& name, const std::string& avail, const std::string& comment);
    void offerFloat(const std::string& name, const std::string& avail, const std::string& comment);
    void offerDouble(const std::string& name, const std::string& avail, const std::string& comment);
    void offerAtlasField(const std::string& name, const std::string& avail, const std::string& comment);

    std::vector<std::string> offeredParamNames() const;
    const std::string&       offeredPlumeVersion() const;
    const std::string&       offeredAtlasVersion() const;

    bool isParamOffered(const std::string& name) const;
    const data::ParameterCatalogue& offers() const;

private:

    // internal utility functions
    void insertParam(const data::Parameter& param, data::ParameterCatalogue& catalogue);

    void setParamsFromConfig(const eckit::Configuration& config);

private:
    std::string requestedPlumeVersion_;
    std::string requestedAtlasVersion_;    
    data::ParameterCatalogue requiredParams_;

    std::string offeredPlumeVersion_;
    std::string offeredAtlasVersion_;
    data::ParameterCatalogue offeredParams_;
};


}  // namespace plume