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

#include "eckit/exception/Exceptions.h"
#include "eckit/log/Log.h"

#include <plume/data/ModelData.h>


namespace plume {
namespace data {


ModelData::ModelData() {}

ModelData::~ModelData() {
    // Nothing to do here (each parameter destructs its data pointer, as appropriate..)
}




// ----------------- creators -------------------
void ModelData::createInt(std::string name, int valInit) {
    ValueEntry entry(name, new ParameterValueIntOwning( new int{valInit} ));
    insertValue(entry);
}

void ModelData::createBool(std::string name, bool valInit) {
    ValueEntry entry(name, new ParameterValueBoolOwning( new bool{valInit} ));
    insertValue(entry);
}

void ModelData::createFloat(std::string name, float valInit) {
    ValueEntry entry(name, new ParameterValueFloatOwning( new float{valInit} ));
    insertValue(entry);
}

void ModelData::createDouble(std::string name, double valInit) {
    ValueEntry entry(name, new ParameterValueDoubleOwning( new double{valInit} ));
    insertValue(entry);
}

void ModelData::createString(std::string name, char* valInit) {
    ValueEntry entry(name, new ParameterValueStringOwning( new std::string{valInit} ));
    insertValue(entry);
}



// ----------------- inserters -----------------

// insert int
void ModelData::provideInt(std::string name, int* val) {    
    ValueEntry entry(name, new ParameterValueInt(val));
    insertValue(entry);
}


// insert bool
void ModelData::provideBool(std::string name, bool* val) {
    ValueEntry entry(name, new ParameterValueBool(val));
    insertValue(entry);
}


// insert float
void ModelData::provideFloat(std::string name, float* val) {
    ValueEntry entry(name, new ParameterValueFloat(val));
    insertValue(entry);
}


// insert double
void ModelData::provideDouble(std::string name, double* val) {
    ValueEntry entry(name, new ParameterValueDouble(val));
    insertValue(entry);
}


// insert string
void ModelData::provideString(std::string name, char* val) {
    ValueEntry entry(name, new ParameterValueString(val));
    insertValue(entry);
}

// insert a Pointer to an Atlas field
void ModelData::provideAtlasFieldShared(std::string name, atlas::Field::Implementation* field_ptr) {
    ASSERT_MSG(field_ptr->bytes() >= 0, "Provided Atlas field not readable!");
    ValueEntry entry(name, new ParameterValueAtlasField(field_ptr));
    insertValue(entry);
}


// --- Updaters
void ModelData::updateInt(std::string name, int val) {
    ASSERT(valueMap_.at(name)->owns());
    valueMap_.at(name)->set<int>(val);
}

void ModelData::updateBool(std::string name, bool val) {
    ASSERT(valueMap_.at(name)->owns());
    valueMap_.at(name)->set<bool>(val);
}

void ModelData::updateFloat(std::string name, float val) {
    ASSERT(valueMap_.at(name)->owns());
    valueMap_.at(name)->set<float>(val);
}

void ModelData::updateDouble(std::string name, double val) {
    ASSERT(valueMap_.at(name)->owns());
    valueMap_.at(name)->set<double>(val);
}

void ModelData::updateString(std::string name, std::string val) {
    ASSERT(valueMap_.at(name)->owns());
    valueMap_.at(name)->set<std::string>(val);
}



// --- getters

int ModelData::getInt(std::string name) const {
    auto entry = valueMap_.at(name);
    ASSERT(entry->type() == ParameterType::INT);
    return int{*entry->as<int*>()};
}

bool ModelData::getBool(std::string name) const {
    auto entry = valueMap_.at(name);
    ASSERT(entry->type() == ParameterType::BOOL);
    return bool{*entry->as<bool*>()};
}

float ModelData::getFloat(std::string name) const {
    auto entry = valueMap_.at(name);
    ASSERT(entry->type() == ParameterType::FLOAT);
    return float{*entry->as<float*>()};
}

double ModelData::getDouble(std::string name) const {
    auto entry = valueMap_.at(name);
    ASSERT(entry->type() == ParameterType::DOUBLE);
    return double{*entry->as<double*>()};
}

std::string ModelData::getString(std::string name) const {
    auto entry = valueMap_.at(name);
    ASSERT(entry->type() == ParameterType::STRING);
    return std::string{entry->as<char*>()};
}

// Get Field
atlas::Field ModelData::getAtlasFieldShared(std::string name) const {
    auto entry = valueMap_.at(name);
    ASSERT(entry->type() == ParameterType::ATLAS_FIELD);
    return atlas::Field(entry->as<atlas::Field::Implementation*>());
}


// Get a subset of the ModelData
ModelData ModelData::filter(std::vector<std::string> params) const {
    ModelData filteredData;
    std::vector<std::string> availParams = getAvailableValues();
    for (const auto& key : params) {
        if (std::find(availParams.begin(), availParams.end(), key) != availParams.end()) {
            auto entry = valueMap_.at(key);
            filteredData.valueMap_.insert( std::make_pair(key, entry));
        } else {
            eckit::Log::info() << "Parameter: " << key << " NOT found in Data! " << std::endl;
        }
    }
    return filteredData;
}

// Return a subset of the ModelData (from catalogue)
ModelData ModelData::filter(ParameterCatalogue params) const {
    return filter(params.getParamNames());
}


bool ModelData::hasParameter(const std::string& name, const ParameterType& type) const {
    if (valueMap_.find(name) != valueMap_.end()) {
        ASSERT_MSG(valueMap_.at(name)->type() == type, "value.type = " + ParameterTypeConverter::toString(valueMap_.at(name)->type()) 
            + " vs expected = " + ParameterTypeConverter::toString(type));
        return true;
    }
    return false;
}


void ModelData::print() const {
    eckit::Log::info() << "*** Parameters: " << std::endl;
    for (auto k : valueMap_) {
        eckit::Log::info() << "Param: " << k.first << std::endl;
    }
}


// -------- private

// All values available
std::vector<std::string> ModelData::getAvailableValues() const {
    std::vector<std::string> keys;
    for (const auto& key : valueMap_) {
        keys.push_back(key.first);
    }
    return keys;
}

void ModelData::insertValue(const ValueEntry& entry) {
    auto inserted = valueMap_.insert(entry).second;
    if (!inserted) {
        eckit::Log::warning() << "Parameter: " << entry.first << " already present in Metadata. Not inserted!" << std::endl;
    }
}


}  // namespace data
}  // namespace plume