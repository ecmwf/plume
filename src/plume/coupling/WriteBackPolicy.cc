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

#include "plume/coupling/WriteBackPolicy.h"

#include "eckit/exception/Exceptions.h"

namespace plume {

WriteBackPolicy writeBackPolicyFromString(const std::string& str) {
    if (str == "disabled")              return WriteBackPolicy::disabled;
    if (str == "single-writer")         return WriteBackPolicy::single_writer;
    if (str == "single-writer-strict")  return WriteBackPolicy::single_writer_strict;
    if (str == "multi-writer")          return WriteBackPolicy::multi_writer;
    if (str == "multi-writer-strict")   return WriteBackPolicy::multi_writer_strict;
    throw eckit::BadValue(
        "Unknown write-back-policy \"" + str + "\". "
        "Valid values are: \"disabled\", \"single-writer\", \"single-writer-strict\", "
        "\"multi-writer\", \"multi-writer-strict\".",
        Here());
}

std::ostream& operator<<(std::ostream& os, WriteBackPolicy policy) {
    switch (policy) {
        case WriteBackPolicy::disabled:             return os << "disabled";
        case WriteBackPolicy::single_writer:        return os << "single-writer";
        case WriteBackPolicy::single_writer_strict: return os << "single-writer-strict";
        case WriteBackPolicy::multi_writer:         return os << "multi-writer";
        case WriteBackPolicy::multi_writer_strict:  return os << "multi-writer-strict";
    }
    throw eckit::BadValue("Unknown WriteBackPolicy enum value", Here());
}

}  // namespace plume
