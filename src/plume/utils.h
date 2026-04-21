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
#include <ostream>

#include "eckit/utils/StringTools.h"
#include "eckit/exception/Exceptions.h"

#include "eckit/config/Configuration.h"


namespace plume {

/** @brief A simple class to represent a library version, and to compare versions.
 *
 */
class LibVersion {
    public:
        LibVersion(std::string versionString) : versionString_{versionString} {

            std::vector<std::string> ver_vec;

            try {                
                ver_vec = eckit::StringTools::split(".", versionString);
            } catch (eckit::Exception& e) {
                throw eckit::BadValue("LibVersion string " + versionString + " not parsable! => " + e.what(), Here());
            }

            major_ = ver_vec[0];
            minor_ = ver_vec[1];
            patch_ = ver_vec[2];
        }

        const std::string& asString() const {
            return versionString_;
        }

        int asInt() const {
            return 10000 * std::stoi(major_) + 100 * std::stoi(minor_) + 1 * std::stoi(patch_);
        }

        bool operator==(const LibVersion& rhs) const {
            return asInt() == rhs.asInt();
        }

        bool operator!=(const LibVersion& rhs) const {
            return asInt() != rhs.asInt();
        }        

        bool operator>(const LibVersion& rhs) const {
            return asInt() > rhs.asInt();
        }

        bool operator<(const LibVersion& rhs) const {
            return asInt() < rhs.asInt();
        }

        bool operator>=(const LibVersion& rhs) const {
            return asInt() >= rhs.asInt();
        }

        bool operator<=(const LibVersion& rhs) const {
            return asInt() <= rhs.asInt();
        }

        friend std::ostream& operator<<(std::ostream& oss, const LibVersion& obj) {
            oss << obj.asString();
            return oss;
        }

    private:

        std::string versionString_;
        std::string major_;
        std::string minor_;
        std::string patch_;
};

inline void indent(std::ostream& os, int n) {
    for (int i = 0; i < n; i++) os << "  ";
}

/**
 * @brief Recursively serialize an eckit configuration into a JSON-like string.
 */
inline void configToJson(const eckit::Configuration& cfg, std::ostream& os, int userIndent) {

    os << "{\n";

    auto keys = cfg.keys();

    // Emit each key/value pair in order.
    for (size_t i = 0; i < keys.size(); ++i) {

        const auto& key = keys[i];

        // Indent one level inside current object.
        indent(os, userIndent + 1);
        os << "\"" << key << "\": ";

        // Branch by value kind: single sub-config, list of sub-configs, or scalar.
        if (cfg.isSubConfiguration(key)) {

            // Recurse into nested object.
            auto child = cfg.getSubConfiguration(key);
            configToJson(child, os, userIndent + 1);

        } else if (cfg.isSubConfigurationList(key)) {

            // Recurse into an array of nested objects.
            auto childList = cfg.getSubConfigurations(key);

            os << "[\n";
            for (size_t j = 0; j < childList.size(); ++j) {
                indent(os, userIndent + 2);
                configToJson(childList[j], os, userIndent + 2);
                if (j + 1 < childList.size()) os << ",";
                os << "\n";
            }

            // Indent closing array bracket.
            indent(os, userIndent + 1);
            os << "]";

        } else {

            // Scalar fallback: represent scalar as a quoted string.
            std::string value = cfg.getString(key);
            os << "\"" << value << "\"";
        }

        if (i + 1 < keys.size()) os << ",";
        os << "\n";
    }

    indent(os, userIndent);
    os << "}";
};

} // namespace plume
 