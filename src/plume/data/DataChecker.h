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
#include "plume/data/ModelData.h"
#include "plume/data/ParameterCatalogue.h"


namespace plume {
namespace data {


class CheckPolicy {
public:
    CheckPolicy() = default;
    virtual ~CheckPolicy() = default;
    virtual void react(const std::string& errStr) const = 0;
};


class CheckPolicyWarning : public CheckPolicy {
public:
    virtual void react(const std::string& errStr) const {
        eckit::Log::warning() << errStr << std::endl;
    };
};


class CheckPolicyThrow : public CheckPolicy {
public:
    virtual void react(const std::string& errStr) const {
        throw eckit::BadValue(errStr, Here());
    };
};


/**
 * @brief Check data against a catalogue
 * 
 */
class DataChecker {

public:

    DataChecker();
    virtual ~DataChecker();

    static void checkAllParams(const ModelData& data, const ParameterCatalogue& catalogue, const CheckPolicy& policy);
    static void checkAlwaysAvailParams(const ModelData& data, const ParameterCatalogue& catalogue, const CheckPolicy& policy);

private:
    static std::string errorStr(const std::string& param);
};


} // namespace data
} // namespace plume
