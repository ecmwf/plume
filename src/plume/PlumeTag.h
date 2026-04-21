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

namespace plume {

// Define PlumeTag enum class from TagDefinitions.def
enum class PlumeTag {
#define PLUME_TAG(tag_id, tag_name) tag_id, // just list tag_id's here to define the enum entries
#include "TagDefinitions.def"
#undef PLUME_TAG
};

std::string plumeTagToString(PlumeTag tag);
PlumeTag plumeTagFromString(const std::string& tag);

}  // namespace plume
