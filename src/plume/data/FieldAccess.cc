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
#include <utility>

#include "plume/coupling/WriteBackLedger.h"
#include "plume/data/FieldAccess.h"

namespace plume {
namespace data {

// -----------------------------------------------------------------------------
// WriteScope — the in-place write-back RAII primitive (declared in FieldAccess.h).

// The scope is deliberately decoupled from ModelData: ModelData::writeParam has already staged the write and resolved
// the buffer, so the scope only borrows the ledger (to report an aborted write) and the model buffer pointer.

WriteScope::WriteScope(coupling::WriteBackLedger& ledger, std::string name, atlas::Field& field) :
    ledger_(&ledger), name_(std::move(name)), field_(&field) {}

WriteScope::WriteScope(WriteScope&& other) noexcept :
    ledger_(other.ledger_),
    name_(std::move(other.name_)),
    field_(other.field_),
    valid_(other.valid_),
    committed_(other.committed_) {
    // Neutralise the moved-from scope so its destructor neither aborts nor reports.
    other.ledger_    = nullptr;
    other.field_     = nullptr;
    other.valid_     = false;
    other.committed_ = true;
}

WriteScope::~WriteScope() {
    if (ledger_ != nullptr && !committed_) {
        valid_ = false;  // poison any outstanding FieldWriter before reporting
        try {
            ledger_->reportError(name_, "write scope destroyed without commit()");
        }
        catch (...) {
            // A destructor must never throw; the ledger error is best-effort here.
        }
    }
}

void WriteScope::commit() {
    // The in-place mutation has already landed in the model buffer through the FieldWriter; there is no
    // separate buffer to flush. Committing simply poisons the handle and leaves the ledger slot STAGED for
    // the model to flush at end of cycle — exactly like the value-based writeParam path.
    valid_     = false;
    committed_ = true;
}

}  // namespace data
}  // namespace plume
