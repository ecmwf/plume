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
// #include <any>
#include <string>
#include <vector>
#include <functional>

#include "eckit/value/Value.h"

#include "atlas/field/Field.h"
#include "atlas/field/detail/FieldImpl.h"


#include "plume/data/ParameterCatalogue.h"


namespace plume {
namespace data {


// Container class for Values and pointers
class ModelData {

    using ValueEntry = std::pair<std::string, std::shared_ptr<ParameterValue>>;
    using ValueMap   = std::map<std::string, std::shared_ptr<ParameterValue>>;

public:
    ModelData();

    ~ModelData();

    // --- creates (new values)
    void createInt(std::string name, int valInit);
    void createBool(std::string name, bool valInit);
    void createFloat(std::string name, float valInit);
    void createDouble(std::string name, double valInit);
    void createString(std::string name, char* valInit);

    // --- provides (existing values)
    void provideInt(std::string name, int* val);
    void provideBool(std::string name, bool* val);
    void provideFloat(std::string name, float* val);
    void provideDouble(std::string name, double* val);
    void provideString(std::string name, char* val);
    void provideAtlasFieldShared(std::string name, atlas::Field::Implementation* field_ptr);

    // --- Data providers can "update" created parameters
    void updateInt(std::string name, int val);
    void updateBool(std::string name, bool val);
    void updateFloat(std::string name, float val);
    void updateDouble(std::string name, double val);
    void updateString(std::string name, std::string val);

    // --- Data users can "update" their local view of the data parameters
    int          getInt(std::string name) const ;
    bool         getBool(std::string name) const ;
    float        getFloat(std::string name) const ;
    double       getDouble(std::string name) const ;
    std::string  getString(std::string name) const ;

    // -- shared data
    atlas::Field getAtlasFieldShared(std::string name) const;

    // Return a subset of the ModelData
    ModelData filter(std::vector<std::string> params) const ;

    // Return a subset of the ModelData
    ModelData filter(ParameterCatalogue params) const ;

    // check if a parameter is in the data
    bool hasParameter(const std::string& name) const ;
    bool hasParameter(const std::string& name, const ParameterType& type) const ;

    // list available parameters of a certain type
    std::vector<std::string> listAvailableParameters(std::string type_string) const ;

    void print() const;

private:

    void insertValue(const ValueEntry& entry);

    std::vector<std::string> getAvailableValues() const;

    // Values
    ValueMap valueMap_;

    // // value map
    // ValueMapAny valueMapAny_;

};

}  // namespace data
}  // namespace plume