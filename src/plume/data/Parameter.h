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
#include <string>
#include <vector>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/exception/Exceptions.h"

#include "atlas/field/Field.h"
#include "atlas/field/detail/FieldImpl.h"

#include "plume/Configurable.h"

namespace plume {
namespace data {


/**
 * @brief Used for strong typing
 * 
 */
enum class ParameterType {
    INT,
    BOOL,
    FLOAT,
    DOUBLE,
    STRING,
    ATLAS_FIELD
};


/**
 * @brief Convert data types to/from string
 * 
 */
class ParameterTypeConverter {
public:
    static std::string toString(ParameterType type) {
        switch (type) {
          case (ParameterType::INT):
            return "INT";
            break;
          case (ParameterType::BOOL):
            return "BOOL";
            break;
          case (ParameterType::FLOAT):
            return "FLOAT";
            break;
          case (ParameterType::DOUBLE):
            return "DOUBLE";
            break;
          case (ParameterType::STRING):
            return "STRING";
            break;            
          case (ParameterType::ATLAS_FIELD):
            return "ATLAS_FIELD";
            break;
          default:
            throw(eckit::BadValue("Parameter Type not recognised!"));
        }
    }
    static ParameterType fromString(std::string typeStr) {
        if (typeStr == "INT") {
            return ParameterType::INT;
        } else if (typeStr == "BOOL") {
            return ParameterType::BOOL; 
        } else if (typeStr == "FLOAT") {
            return ParameterType::FLOAT; 
        } else if (typeStr == "DOUBLE") {
            return ParameterType::DOUBLE;
        } else if (typeStr == "STRING") {
            return ParameterType::STRING;             
        } else if (typeStr == "ATLAS_FIELD") {
            return ParameterType::ATLAS_FIELD;
        } else {
            throw(eckit::BadValue("Parameter Type not recognised!"));
        }
    }

    static std::vector<std::string> toStringVector() {
        return std::vector<std::string>{"INT", "BOOL", "FLOAT", "DOUBLE", "STRING", "ATLAS_FIELD"};
    }
};


/**
 * @brief A parameter of the data
 * 
 */
class Parameter : public CheckedConfigurable {

public:

    Parameter(const eckit::Configuration& config);
    Parameter(const std::string& name, const std::string& type, const std::string& available="", const std::string& comment = "");
    Parameter(const std::string& name, const ParameterType& type, const std::string& available="", const std::string& comment = "");

    ~Parameter();

    bool operator==(const Parameter& other) {
        return (name()==other.name() && type()==other.type());
    }

    const std::string& name() const;
    const plume::data::ParameterType& type() const;
    const std::string& available() const;
    const std::string& comment() const;

private:

    eckit::LocalConfiguration params2config(const std::string& name,  const std::string& type, const std::string& available, const std::string& comment);

private:

    std::string name_; // parameter name
    plume::data::ParameterType dataType_; // parameter type
    std::string available_; // parameter availability ["always", "on-request"]
    std::string comment_; // optional comment

};


// Generic interface for a parameter value
class ParameterValue {

public:

    ParameterValue(void* valuePtr, ParameterType type) : 
        contentPtr_{valuePtr},
        type_{type} {}
        
    virtual ~ParameterValue(){};
    ParameterValue(const ParameterValue& other)  = delete;
    ParameterValue& operator=(const ParameterValue& other) = delete;

    virtual std::string toString() const {return ParameterTypeConverter::toString(type_);}
    virtual ParameterType type() const {return type_;}

    // owns the underlying data?
    virtual bool owns() const = 0;

    /// TODO: this is unsafe, temporary solution only!
    /// use std::variant or std::any instead?
    template <typename T>
    T as() const {
        return reinterpret_cast<T>(contentPtr_);
    }

    template <typename T>
    void set(T value) {
        *(reinterpret_cast<T*>(contentPtr_)) = value;
    }

protected:
    ParameterType type_;
    void* contentPtr_;

};



// --------------------- Non-owning param values ---------------------
class ParameterValueInt : public ParameterValue {
public:
    explicit ParameterValueInt(int* ptr) : ParameterValue{ptr, ParameterType::INT} {}
    virtual bool owns() const { return false; }
};

class ParameterValueBool : public ParameterValue {
public:
    explicit ParameterValueBool(bool* ptr) : ParameterValue{ptr, ParameterType::BOOL} {}
    virtual bool owns() const { return false; }
};

class ParameterValueFloat : public ParameterValue {
public:
    explicit ParameterValueFloat(float* ptr) : ParameterValue{ptr, ParameterType::FLOAT} {}
    virtual bool owns() const { return false; }
};

class ParameterValueDouble : public ParameterValue {
public:
    explicit ParameterValueDouble(double* ptr) : ParameterValue{ptr, ParameterType::DOUBLE} {}
    virtual bool owns() const { return false; }
};

class ParameterValueString : public ParameterValue {
public:
    explicit ParameterValueString(char* ptr) : ParameterValue{ptr, ParameterType::STRING} {}
    virtual bool owns() const { return false; }
};


// --------------------- Owning param values ---------------------
class ParameterValueIntOwning : public ParameterValue {
public:
    explicit ParameterValueIntOwning(int* ptr) : ParameterValue{ptr, ParameterType::INT} {}
    virtual ~ParameterValueIntOwning() {
        delete static_cast<int*>(contentPtr_);
        contentPtr_=nullptr;
    }
    virtual bool owns() const { return true; }
};

class ParameterValueBoolOwning : public ParameterValue {
public:
    explicit ParameterValueBoolOwning(bool* ptr) : ParameterValue{ptr, ParameterType::BOOL} {}
    virtual ~ParameterValueBoolOwning() {
        delete static_cast<bool*>(contentPtr_);
        contentPtr_=nullptr;
    }
    virtual bool owns() const { return true; }
};

class ParameterValueFloatOwning : public ParameterValue {
public:
    explicit ParameterValueFloatOwning(float* ptr) : ParameterValue{ptr, ParameterType::FLOAT} {}
    virtual ~ParameterValueFloatOwning() {
        delete static_cast<float*>(contentPtr_);
        contentPtr_=nullptr;
    }
    virtual bool owns() const { return true; }
};

class ParameterValueDoubleOwning : public ParameterValue {
public:
    explicit ParameterValueDoubleOwning(double* ptr) : ParameterValue{ptr, ParameterType::DOUBLE} {}
    virtual ~ParameterValueDoubleOwning() {
        delete static_cast<double*>(contentPtr_);
        contentPtr_=nullptr;
    }
    virtual bool owns() const { return true; }
};

class ParameterValueStringOwning : public ParameterValue {
public:
    explicit ParameterValueStringOwning(std::string* ptr) : ParameterValue{ptr, ParameterType::STRING} {}
    virtual ~ParameterValueStringOwning() {
        delete static_cast<std::string*>(contentPtr_);
        contentPtr_=nullptr;
    }
    virtual bool owns() const { return true; }
};


// --------------------- Shared param values ---------------------
class ParameterValueAtlasField : public ParameterValue {
public:
    explicit ParameterValueAtlasField(atlas::Field::Implementation* ptr) : ParameterValue{ptr, ParameterType::ATLAS_FIELD} {};
    virtual bool owns() const { return false; }
};


}  // namespace data
}  // namespace plume