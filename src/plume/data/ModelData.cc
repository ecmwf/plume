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
#include <algorithm>
#include <exception>

#include <plume/data/ModelData.h>


namespace plume {
namespace data {


ModelData::ModelData() {
    // registering update strategies
    registerStrategy<field_provider::WindAtHeight>();
}


// Get a subset of the ModelData
ModelData ModelData::filter(std::set<std::string> params) const {
    ModelData filteredData;
    std::vector<std::string> availParams = getAvailableValues();
    for (const auto& key : params) {
        if (std::find(availParams.begin(), availParams.end(), key) != availParams.end()) {
            auto entry = valueMap_.at(key);
            filteredData.valueMap_.insert(std::make_pair(key, entry));
        }
        else {
            eckit::Log::info() << "Parameter: " << key << " NOT found in Data! " << std::endl;
        }
    }
    return filteredData;
}


// Return a subset of the ModelData (from catalogue)
ModelData ModelData::filter(ParameterCatalogue params) const {
    return filter(params.getParamNames());
}


// Check if a parameter is in the data
bool ModelData::hasParameter(const std::string& name) const {
    return valueMap_.find(name) != valueMap_.end();
}


bool ModelData::hasParameter(const std::string& name, const std::string& level, const std::string& levtype) const {
    std::string entryName = IParameterObserver::deriveParamName(name, levtype, level);
    return hasParameter(entryName);
}


bool ModelData::hasParameter(const std::string& name, const ParameterType& type) const {
    if (valueMap_.find(name) != valueMap_.end()) {
        ASSERT_MSG(valueMap_.at(name)->type() == type,
                   "value.type = " + std::string(typeToString(valueMap_.at(name)->type())) +
                       " vs expected = " + std::string(typeToString(type)));
        return true;
    }
    return false;
}


bool ModelData::isUpdated(const std::string& name) const {
    ASSERT_MSG(valueMap_.find(name) != valueMap_.end(), "Element not found in model data: " + name);
    return valueMap_.at(name)->isUpdated();
}

bool ModelData::isUpdated(const std::string& name, const std::string& level, const std::string& levtype) const {
    std::string entryName = IParameterObserver::deriveParamName(name, levtype, level);
    return isUpdated(entryName);
}


void ModelData::setUpdated(const std::vector<std::string>& params) {
    clearUpdated();
    for (const auto& name : params) {
        ASSERT_MSG(valueMap_.find(name) != valueMap_.end(), "Element not found in model data: " + name);
        valueMap_.at(name)->setUpdated(true);
    }
}


void ModelData::clearUpdated() {
    for (const auto& [param, value] : valueMap_) {
        value->setUpdated(false);
    }
}


void ModelData::print() const {
    eckit::Log::info() << "*** Parameters: " << std::endl;
    for (auto k : valueMap_) {
        eckit::Log::info() << "Param: " << k.first << std::endl;
    }
}


std::vector<std::string> ModelData::listAvailableParameters(std::string type_string) const {
    ParameterType type = typeFromString(type_string.c_str());
    std::vector<std::string> keys;
    for (const auto& key : valueMap_) {
        if (key.second->type() == type) {
            keys.push_back(key.first);
        }
    }
    return keys;
}

// -------- private

void ModelData::addDependency(const std::string& observer, const std::string& observable, const std::string& type,
                              const eckit::Configuration& config) {
    // 1. attach observer to observable
    auto publisher = std::dynamic_pointer_cast<IParameterObservable>(valueMap_.at(observable));
    if (!publisher) {
        throw eckit::BadCast("'" + observable + "' is not an Observable parameter!", Here());
    }
    auto subscriber = std::dynamic_pointer_cast<IParameterObserver>(valueMap_.at(observer));
    if (!subscriber) {
        throw eckit::BadCast("'" + observer + "' is an observable, it cannot subscribe to other parameters!", Here());
    }
    subscriber->setSubject(publisher);
    // 2. parse config and build arg list
    field_provider::StrategyArgList strategyArgs = strategyHelpers_.at(type)(config, valueMap_, observable, observer);
    // 3. create update strategy
    auto strategy = strategyRegistry_.at(type)(strategyArgs);
    // 4. set initial observer value
    strategy->update();
    valueMap_.at(observer)->setUpdated(false);
    // 5. attach strategy to observer
    subscriber->setUpdateStrategy(std::move(strategy));
}


// All values available
std::vector<std::string> ModelData::getAvailableValues() const {
    std::vector<std::string> keys;
    for (const auto& key : valueMap_) {
        keys.push_back(key.first);
    }
    return keys;
}

std::unique_ptr<field_provider::UpdateStrategy> ModelData::createStrategy(const std::string& type,
                                                                          const field_provider::StrategyArgList& args) {
    auto it = strategyRegistry_.find(type);
    if (it == strategyRegistry_.end()) {
        throw eckit::BadValue("Unknown update strategy: " + type, Here());
    }
    return it->second(args);
}

void ModelData::dispatchCreateParam(const std::string& strategy, const eckit::Configuration& config) {
    ParameterType type = typeFromString(config.getString("type").c_str());
    switch (type) {
        case ParameterType::INT:
            return createParam<int>(strategy, config);
        case ParameterType::BOOL:
            return createParam<bool>(strategy, config);
        case ParameterType::FLOAT:
            return createParam<float>(strategy, config);
        case ParameterType::DOUBLE:
            return createParam<double>(strategy, config);
        case ParameterType::STRING:
            return createParam<std::string>(strategy, config);
        case ParameterType::ATLAS_FIELD:
            return createParam<atlas::Field>(strategy, config);
        default:
            throw eckit::BadValue("Parameter Type invalid or not recognised!", Here());
    }
}


}  // namespace data
}  // namespace plume