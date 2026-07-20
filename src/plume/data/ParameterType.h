/*
 * (C) Copyright 2025- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 *
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */
#pragma once

#include <cstring>
#include <string>
#include <type_traits>

#include "eckit/exception/Exceptions.h"

#include "atlas/field/Field.h"

namespace plume {
namespace data {

/**
 * @brief Used for strong typing
 *
 */
enum class ParameterType
{
    INT,
    BOOL,
    FLOAT,
    DOUBLE,
    STRING,
    ATLAS_FIELD,
    INVALID
};

/**
 * @brief Primary type-to-enum mapping trait.
 *
 * The primary template defaults to ParameterType::INVALID, which indicates that the type is unsupported.
 * Supported types must specialize this template and provide a concrete ParameterType.
 *
 * @tparam T The type to be mapped.
 */
template <typename T>
struct DeduceParameterType {
    static constexpr ParameterType value = ParameterType::INVALID;
    static constexpr const char* name    = "INVALID";
};

template <>
struct DeduceParameterType<int> {
    static constexpr ParameterType value = ParameterType::INT;
    static constexpr const char* name    = "INT";
};

template <>
struct DeduceParameterType<size_t> : DeduceParameterType<int> {};

template <>
struct DeduceParameterType<bool> {
    static constexpr ParameterType value = ParameterType::BOOL;
    static constexpr const char* name    = "BOOL";
};

template <>
struct DeduceParameterType<float> {
    static constexpr ParameterType value = ParameterType::FLOAT;
    static constexpr const char* name    = "FLOAT";
};

template <>
struct DeduceParameterType<double> {
    static constexpr ParameterType value = ParameterType::DOUBLE;
    static constexpr const char* name    = "DOUBLE";
};

template <>
struct DeduceParameterType<char> {
    static constexpr ParameterType value = ParameterType::STRING;
    static constexpr const char* name    = "STRING";
};

template <>
struct DeduceParameterType<std::string> : DeduceParameterType<char> {};

template <>
struct DeduceParameterType<atlas::Field> {
    static constexpr ParameterType value = ParameterType::ATLAS_FIELD;
    static constexpr const char* name    = "ATLAS_FIELD";
};

/**
 * @brief Deduce the ParameterType associated with the given template type parameter.
 *
 * If the type is not supported (i.e. DeduceParameterType<T>::value evaluates to ParameterType::INVALID),
 * compilation fails with a clear diagnostic. The check is performed entirely at compile time.
 *
 * @tparam T The type to deduce.
 */
template <typename T>
constexpr ParameterType deduceType() {
    static_assert(DeduceParameterType<T>::value != ParameterType::INVALID, "Parameter Type not supported!");
    return DeduceParameterType<T>::value;
}

/**
 * @brief Convert a ParameterType to its string representation.
 */
inline const char* typeToString(ParameterType t) {
    switch (t) {
        case ParameterType::INT:
            return DeduceParameterType<int>::name;
        case ParameterType::BOOL:
            return DeduceParameterType<bool>::name;
        case ParameterType::FLOAT:
            return DeduceParameterType<float>::name;
        case ParameterType::DOUBLE:
            return DeduceParameterType<double>::name;
        case ParameterType::STRING:
            return DeduceParameterType<std::string>::name;
        case ParameterType::ATLAS_FIELD:
            return DeduceParameterType<atlas::Field>::name;
        default:
            throw eckit::BadValue("Parameter Type invalid or not recognised!", Here());
    }
}

/**
 * @brief Convert a string representation to a ParameterType.
 */
inline ParameterType typeFromString(const char* s) {
    if (std::strcmp(s, DeduceParameterType<int>::name) == 0)
        return DeduceParameterType<int>::value;
    if (std::strcmp(s, DeduceParameterType<bool>::name) == 0)
        return DeduceParameterType<bool>::value;
    if (std::strcmp(s, DeduceParameterType<float>::name) == 0)
        return DeduceParameterType<float>::value;
    if (std::strcmp(s, DeduceParameterType<double>::name) == 0)
        return DeduceParameterType<double>::value;
    if (std::strcmp(s, DeduceParameterType<std::string>::name) == 0)
        return DeduceParameterType<std::string>::value;
    if (std::strcmp(s, DeduceParameterType<atlas::Field>::name) == 0)
        return DeduceParameterType<atlas::Field>::value;
    throw eckit::BadValue(std::string("Parameter Type '") + s + "' invalid or not recognised!", Here());
}

}  // namespace data
}  // namespace plume