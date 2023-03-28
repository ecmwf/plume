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
#include "eckit/exception/Exceptions.h"
#include "plume/data/DataChecker.h"


namespace plume {
namespace data {


DataChecker::DataChecker() {

}


DataChecker::~DataChecker() {

}


void DataChecker::checkAllParams(const ModelData& data, const ParameterCatalogue& catalogue, const CheckPolicy& policy) {
    for (const auto& param: catalogue.getParams()) {
        if (data.hasParameter(param.name(), param.type()) == false) {
            policy.react(errorStr(param.name()) );
        }
    }
}


void DataChecker::checkAlwaysAvailParams(const ModelData& data, const ParameterCatalogue& catalogue, const CheckPolicy& policy) {
    for (const auto& param: catalogue.getParams()) {
        if (param.available() == "always") {
            if (data.hasParameter(param.name(), param.type()) == false) {
                policy.react(errorStr(param.name()) );
            }
        }
    }
}


std::string DataChecker::errorStr(const std::string& param) {
    return std::string("Parameter " + param + " is requested but NOT found in data!");
}


} // namespace data
} // namespace plume