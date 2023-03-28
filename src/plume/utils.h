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


namespace plume {

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

} // namespace plume
 