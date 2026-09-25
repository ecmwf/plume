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

#include <cstdint>
#include <ostream>
#include <string>

namespace plume {

/**
 * @brief Policy governing whether and how plugins may write back to model parameters.
 *
 * Rejection in strict mode always applies to the plugin that arrives second in the
 * negotiation loop (= second in the manager config `plugins` list), regardless of
 * whether that plugin is the writer or the reader.  Place the plugin you want to
 * keep first in the config list to guarantee it survives strict-mode negotiation.
 *
 * disabled                (default)
 *   No plugin may request write access to any parameter.
 *
 * single_writer           ("single-writer")
 *   At most one writer per param. A second writer is rejected.
 *   Readers and derived-readers of a written param are ALLOWED; run order is
 *   logged so the user can reason about what value they will observe.
 *
 * single_writer_strict    ("single-writer-strict")
 *   At most one writer per param. A second writer is rejected.
 *   The plugin that creates a write-read or write-derived-read conflict is also
 *   REJECTED (whichever of the writer or the reader appears second in the config).
 *
 * multi_writer            ("multi-writer")
 *   Multiple writers per param are allowed; run order is logged.
 *   Readers and derived-readers of a written param are ALLOWED; run order is logged.
 *
 * multi_writer_strict     ("multi-writer-strict")
 *   Multiple writers per param are allowed; run order is logged.
 *   The plugin that creates a write-read or write-derived-read conflict is
 *   REJECTED (whichever of the writer or the reader appears second in the config).
 *
 * See README for the full policy reference.
 */
enum class WriteBackPolicy : uint8_t {
    disabled             = 0,
    single_writer        = 1,
    single_writer_strict = 2,
    multi_writer         = 3,
    multi_writer_strict  = 4,
};

WriteBackPolicy writeBackPolicyFromString(const std::string& str);

std::ostream& operator<<(std::ostream& os, WriteBackPolicy policy);

/// Returns true for any policy that permits write-back (i.e. not disabled).
inline bool isWriteBackEnabled(WriteBackPolicy p) {
    return p != WriteBackPolicy::disabled;
}

/// Returns true for strict policies that reject coexistence of same param readers and writers.
inline bool isStrictPolicy(WriteBackPolicy p) {
    return p == WriteBackPolicy::single_writer_strict || p == WriteBackPolicy::multi_writer_strict;
}

/// Returns true for policies that allow more than one writer per param.
inline bool isMultiWriter(WriteBackPolicy p) {
    return p == WriteBackPolicy::multi_writer || p == WriteBackPolicy::multi_writer_strict;
}

}  // namespace plume
