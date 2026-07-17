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

namespace plume {
namespace coupling {

class WriteBackTracker;  // forward declaration

/**
 * @class WriteBackKey
 * @brief Passkey token that gates access to write-enable mutators on IParameterValue.
 *
 * Only WriteBackTracker can construct a WriteBackKey, so only WriteBackTracker can call
 * IParameterValue::enableWriteback() and IParameterValue::disableWriteback(). This is
 * enforced at compile time: any other caller cannot name the constructor.
 *
 * Usage:
 * @code
 * // Inside WriteBackTracker only:
 * param.enableWriteback(WriteBackKey{});
 * param.disableWriteback(WriteBackKey{});
 * @endcode
 *
 * @see IParameterValue::enableWriteback, IParameterValue::disableWriteback
 */
class WriteBackKey {
private:
    WriteBackKey() = default;

    friend class WriteBackTracker;
};

}  // namespace coupling
}  // namespace plume
