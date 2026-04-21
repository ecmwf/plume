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

#include "plume/PlumeTag.h"

#include "eckit/exception/Exceptions.h"

namespace plume {

namespace {

// convenience struct to define the mapping between enum values and tag names
struct TagEntry {
    PlumeTag tag;
    const char* name;
};

constexpr TagEntry kTagEntries[] = {
#define PLUME_TAG(tag_id, tag_name) {PlumeTag::tag_id, tag_name},
#include "TagDefinitions.def"
#undef PLUME_TAG
};

std::string validTagsMessage() {
    std::string message = "Valid tags are: ";
    for (std::size_t i = 0; i < (sizeof(kTagEntries) / sizeof(kTagEntries[0])); ++i) {
        if (i > 0) {
            message += ", ";
        }
        message += kTagEntries[i].name;
    }
    message += ".";
    return message;
}

}  // namespace

std::string plumeTagToString(PlumeTag tag) {
    for (const auto& entry : kTagEntries) {
        if (entry.tag == tag) {
            return entry.name;
        }
    }

    throw eckit::BadParameter("Unsupported plume tag enum value.", Here());
}

PlumeTag plumeTagFromString(const std::string& tag) {
    for (const auto& entry : kTagEntries) {
        if (entry.name == tag) {
            return entry.tag;
        }
    }

    throw eckit::BadParameter("Invalid plume tag: '" + tag + "'. " + validTagsMessage(), Here());
}

}  // namespace plume
