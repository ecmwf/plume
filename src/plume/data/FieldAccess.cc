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
#include "plume/data/ParameterValue.h"

namespace plume {
namespace data {

// -----------------------------------------------------------------------------
// WriteScope — the in-place write-back RAII primitive (declared in FieldAccess.h).

WriteScope::WriteScope(coupling::WriteBackLedger& ledger, std::string name,
                       ParameterValueTyped<atlas::Field>& paramValue) :
    ledger_(&ledger), name_(std::move(name)), field_(&paramValue.getSettableField()), paramValue_(&paramValue) {}

WriteScope::WriteScope(WriteScope&& other) noexcept :
    ledger_(other.ledger_),
    name_(std::move(other.name_)),
    field_(other.field_),
    paramValue_(other.paramValue_),
    valid_(other.valid_),
    committed_(other.committed_) {
    // Neutralise the moved-from scope so its destructor neither aborts nor reports.
    other.ledger_     = nullptr;
    other.field_      = nullptr;
    other.paramValue_ = nullptr;
    other.valid_      = false;
    other.committed_  = true;
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
    if (committed_) {
        return;  // idempotent: already finalised
    }
    // Moved-from scopes are an empty husk with nothing to commit — misuse, so this stays a loud failure.
    ASSERT_MSG(paramValue_ != nullptr,
               "WriteScope::commit() called on a moved-from scope for parameter '" + name_ + "'");
    valid_     = false;
    committed_ = true;
    // Notifies any active observers if the parameter is an actively-observed IParameterObservable.
    paramValue_->setUpdated(true);
}

}  // namespace data
}  // namespace plume
