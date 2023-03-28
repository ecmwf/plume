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
#include "Plugin.h"

namespace plume {

Plugin::Plugin(const std::string& name, const std::string& libname) : eckit::system::Plugin(name, libname) {

    eckit::Log::debug() << "Instantiating " << this->name() << std::endl;
}

Plugin::~Plugin() {
    eckit::Log::debug() << "Destroying " << name() << std::endl;
}

}  // namespace plume