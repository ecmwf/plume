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

#include "plume/Manager.h"
#include "plume/coupling/WriteBackLedger.h"

namespace plume::test {

/**
 * @brief Test-only super-accessor for Plume internals.
 *
 * Declared as a friend in Manager and WriteBackLedger to expose private
 * test-only methods (forceReset, clearError) without making them public.
 * Must never be used in production code.
 */
struct ManagerTestAccess {
    static void reset() { plume::Manager::reset(); }

    static void forceLedgerReset(coupling::WriteBackLedger& ledger) { ledger.forceReset(); }
    static void clearLedgerError(coupling::WriteBackLedger& ledger, const std::string& paramName) {
        ledger.clearError(paramName);
    }
};

}  // namespace plume::test