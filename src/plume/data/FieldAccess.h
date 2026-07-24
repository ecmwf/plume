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

#include <ostream>
#include <string>

#include "eckit/exception/Exceptions.h"

#include "atlas/array/Array.h"
#include "atlas/array/DataType.h"
#include "atlas/field/Field.h"
#include "atlas/functionspace/FunctionSpace.h"
#include "atlas/util/Metadata.h"

namespace plume {

namespace coupling {
class WriteBackLedger;  // forward declaration — a WriteScope reports an aborted write back to the ledger.
}

namespace data {

// Plugin-facing field access wrappers.
//
// This header groups the narrow, plugin-facing wrappers over a model field: FieldView is the read-only surface
// returned by getParam; FieldWriter is its write-path sibling returned by writeParam.
// Note this is distinct from FieldProvider, which handles the model-side field-derivation strategy machinery.

/**
 * @brief Read-only, non-owning view over a model field.
 *
 * FieldView exposes only const access to the underlying data (via `operator const atlas::array::Array&`) plus
 * read-only metadata, so plugins can read model fields but cannot mutate them or copy out a mutable handle. It is the
 * return type of getParam<atlas::Field> on the plugin-facing ModelDataView.
 *
 * Because mutability of an atlas view is decided by the const-ness of its source, the only conversion offered here is
 * to `const atlas::array::Array&`: `atlas::array::make_view<T, Rank>(fieldView)` keeps working unchanged but yields an
 * `ArrayView<const T, Rank>` (read-only) even when the caller does not write `const`. There is deliberately NO
 * conversion to `atlas::Field` or to a mutable `atlas::array::Array&`, and no raw-field escape hatch, so a plugin
 * cannot copy out a mutable handle and bypass the write-back ledger. If a plugin does need a mutable field (e.g. for
 * a read-modify-write), it calls clone() to get an independent deep copy and can hand the result to writeParam().
 *
 * @note Scope: this read-only guarantee is C++-ONLY. It applies to plugins holding a ModelDataView (getParam returns
 *       a FieldView). The Fortran/C plugin path (plume_data_get_shared_atlas_field) still hands back a fully mutable
 *       atlas_field sharing the model's buffer. Extending this enforcement to Fortran can follow-up.
 *
 * @note Future extension: the backing handle is an `atlas::Field` today, but this is an implementation detail. If
 *       Plume grows support for other field representations, FieldView can become a builder-style wrapper constructed
 *       from any supported handle type without changing its read-only public surface. The class name is deliberately
 *       representation-agnostic (FieldView, not ConstAtlasFieldView): read-only-ness is its identity, not a variant.
 */
class FieldView {
public:
    /// Wraps an atlas::Field. The copy is a cheap shared-handle copy (shares the underlying FieldImpl/array).
    explicit FieldView(const atlas::Field& field) : field_(field) {}

    /// The one conversion that matters: read-only array access, so atlas::array::make_view works unchanged.
    operator const atlas::array::Array&() const { return field_.array(); }

    // --- Read-only metadata pass-throughs -----------------------------------
    const std::string& name() const { return field_.name(); }
    const atlas::array::ArrayShape& shape() const { return field_.shape(); }
    atlas::idx_t shape(atlas::idx_t i) const { return field_.shape(i); }
    atlas::array::DataType datatype() const { return field_.datatype(); }
    atlas::idx_t rank() const { return field_.rank(); }
    size_t size() const { return field_.size(); }
    atlas::idx_t levels() const { return field_.levels(); }
    const atlas::util::Metadata& metadata() const { return field_.metadata(); }
    const atlas::FunctionSpace& functionspace() const { return field_.functionspace(); }
    atlas::Field clone() const { return field_.clone(); }

    /// Read-only diagnostic streaming — mirrors atlas::Field's operator<< (prints field metadata, never data).
    friend std::ostream& operator<<(std::ostream& os, const FieldView& fieldView) { return os << fieldView.field_; }

    // Deliberately NOT provided: operator atlas::Field / const atlas::Field& field() / operator atlas::array::Array& /
    // mutable metadata() — any of these would let a plugin copy out a handle SHARING the model's FieldImpl and mutate
    // it in place, bypassing the write-back ledger. clone() is the sanctioned way to get a mutable field from a View.

private:
    atlas::Field field_;  // shares the model's FieldImpl (cheap handle copy)
};

/**
 * @brief Mutable, poison-after-scope write handle over a model field — the write-path sibling of FieldView.
 *
 * FieldWriter is what the in-place write-back path hands to an author (see WriteScope / the callable writeParam
 * overload). It exposes exactly one conversion — `operator atlas::array::Array&` — so `atlas::array::make_view<T,
 * Rank>(fieldWriter)` yields a MUTABLE view aliasing the model's own buffer, letting the plugin read-modify-write in
 * place with no scratch allocation. The mutation is only legal because the owning WriteScope has already staged the
 * write with the authorisation ledger; an unauthorised plugin never obtains a FieldWriter at all.
 *
 * Two guardrails keep an already-authorised author on the correct path:
 *   - The handle is non-copyable and non-movable, so it cannot be stashed by value (`auto stolen = f;` does not
 *     compile) and cannot outlive the callback it is passed into.
 *   - Access is gated by a validity flag OWNED BY the WriteScope. On commit/scope-exit the scope flips the flag, so
 *     re-using a captured `FieldWriter&` after the scope throws eckit::BadValue instead of silently touching freed
 *     lifecycle state.
 *
 * Residual gap (deliberately out of scope): an ArrayView taken from a FieldWriter *inside* the scope holds a raw
 * pointer into live model memory; poisoning the flag cannot reach it. This is the determined-author stash, not the
 * casual one — closing it would need a per-access indirection atlas does not provide.
 */
class FieldWriter {
public:
    FieldWriter(const FieldWriter&)            = delete;
    FieldWriter& operator=(const FieldWriter&) = delete;
    FieldWriter(FieldWriter&&)                 = delete;
    FieldWriter& operator=(FieldWriter&&)      = delete;

    /// The one conversion that matters: mutable array access, so atlas::array::make_view yields a writable view.
    operator atlas::array::Array&() {
        ensureValid();
        return field_.array();
    }

    // --- Read-only metadata pass-throughs -----------------------------------
    const std::string& name() const { return field_.name(); }
    const atlas::array::ArrayShape& shape() const { return field_.shape(); }
    atlas::idx_t shape(atlas::idx_t i) const { return field_.shape(i); }
    atlas::array::DataType datatype() const { return field_.datatype(); }
    atlas::idx_t rank() const { return field_.rank(); }
    size_t size() const { return field_.size(); }
    atlas::idx_t levels() const { return field_.levels(); }
    const atlas::util::Metadata& metadata() const { return field_.metadata(); }
    const atlas::FunctionSpace& functionspace() const { return field_.functionspace(); }
    atlas::Field clone() const { return field_.clone(); }

private:
    friend class WriteScope;  // only a WriteScope hands out a FieldWriter into its staged buffer.
    FieldWriter(atlas::Field& field, const bool& valid) : field_(field), valid_(valid) {}

    void ensureValid() const {
        if (!valid_) {
            throw eckit::BadValue("plume::data::FieldWriter used outside its write scope", Here());
        }
    }

    atlas::Field& field_;  // the model's own buffer (aliased, not copied, typically borrowed from the scope)
    const bool& valid_;    // owned by the WriteScope; flipped to false on commit/scope-exit
};

/**
 * @brief Move-only RAII scope for an in-place, copy-free write-back.
 *
 * A WriteScope is obtained from the arity overload writeParam(name) (no value argument) on ModelData / ModelDataView.
 * writeParam stages the write with the authorisation ledger (throwing if the plugin is not authorised or the
 * single-writer policy is violated) before handing back the scope; field() then hands a FieldWriter aliasing
 * the model's own buffer so the author can read-modify-write in place; commit() finalises the scope. If the scope is
 * destroyed without commit() (e.g. the author forgot, or the body threw), the destructor poisons the handle and
 * reports the failure to the ledger so the missing write is not silently lost.
 *
 * @code
 *   auto w = data.writeParam("swh");                          // stage → WriteScope
 *   auto v = atlas::array::make_view<double, 2>(w.field());   // mutable view over the model buffer
 *   v(i, j) *= 1.05;                                          // in-place read-modify-write
 *   w.commit();                                               // finalise (dtor aborts+reports if forgotten)
 * @endcode
 *
 * This is the manual-control escape hatch; most plugin authors use the callable writeParam(name, body) overload, which
 * owns a WriteScope internally and guarantees stage → run → commit / abort-on-throw.
 */
class WriteScope {
public:
    WriteScope(WriteScope&& other) noexcept;
    WriteScope& operator=(WriteScope&&)      = delete;
    WriteScope(const WriteScope&)            = delete;
    WriteScope& operator=(const WriteScope&) = delete;
    ~WriteScope();

    /// Hands out a mutable, poison-after-scope handle aliasing the staged model buffer.
    FieldWriter field() { return FieldWriter{*field_, valid_}; }

    /// Finalises the scope: poison any outstanding FieldWriter and leave the ledger slot staged for the model flush.
    void commit();

private:
    // Only ModelData::writeParam(name) constructs a WriteScope, and only after it has staged the write with the
    // ledger — so a scope can never exist for an unauthorised or unstaged write. The scope itself needs no access
    // to ModelData's internals: it borrows the already-resolved buffer and the ledger (for the abort report).
    friend class ModelData;
    WriteScope(coupling::WriteBackLedger& ledger, std::string name, atlas::Field& field);

    coupling::WriteBackLedger* ledger_;  // null after a move; guards the destructor abort path
    std::string name_;
    atlas::Field* field_;    // the staged model buffer (owned by the model, aliased here)
    bool valid_     = true;  // referenced by every FieldWriter this scope hands out
    bool committed_ = false;
};

}  // namespace data
}  // namespace plume
