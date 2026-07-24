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

#include <type_traits>

#include "plume/data/FieldAccess.h"
#include "plume/data/ModelData.h"

namespace plume {
namespace data {

/**
 * @brief Plugin-facing, read/write-restricted view over a ModelData.
 *
 * ModelDataView is the type handed to plugins (via PluginCore::modelData()). It exposes only the plugin-legal surface
 * of ModelData — reading parameters (getParam), writing model values back through the authorised ledger (writeParam),
 * and querying availability/updated state — while making the model-only mutation entry points (updateParam,
 * provideParam, createParam, ...) and the Manager-facing lifecycle (filter, ledger attach/enroll, acknowledge/pending,
 * ...) inaccessible.
 *
 * Enforcement is structural rather than runtime: ModelDataView privately inherits ModelData, so a plugin cannot slice
 * or cast a view back to a ModelData& to reach the hidden methods — any such attempt is a compile error.
 * Only the members re-published below via `using` are reachable.
 *
 * @note The view borrows nothing: it owns the (filtered) ModelData moved into it, so its lifetime is
 *       self-contained and it carries the write-back ledger pointer and consumer tag set by filter().
 *
 * @note (PLUME-82) A ModelDataView *shares ownership* of the model's parameters: filter() copies each
 *       parameter's std::shared_ptr into the view (see ModelData::filter), so a plugin's view and the
 *       model-facing ModelData co-own the same atlas::Field handles. This paradigm is under review: it
 *       couples plugin-view lifetime to model-parameter (and thus atlas::Field) lifetime, which caused a
 *       teardown heap corruption when a view retained in the process-static PluginRegistry outlived atlas'
 *       thread-local allocator and destroyed its shared fields at program exit. The current mitigation is
 *       PluginHandler::teardown() → PluginCore::releaseData(), which drops the view during finalise() while
 *       atlas is still alive. PLUME-82 tracks making views non-owning observers of model parameters instead,
 *       which would remove the need for that explicit release.
 */
class ModelDataView : private ModelData {
public:
    /// Wraps a (typically filtered) ModelData, taking ownership of it.
    explicit ModelDataView(ModelData data) : ModelData(std::move(data)) {}

    // --- Plugin-legal surface (re-published from the private base) -----------

    // Read parameter values. Scalars are returned by value unchanged; an atlas::Field is returned as a read-only
    // FieldView (never a mutable atlas::Field handle), so a plugin holding only a view cannot copy out a mutable
    // handle and bypass the write-back ledger. The call syntax is unchanged: getParam<atlas::Field>(name) still
    // reads naturally and make_view(...) over the result yields a const view. See ModelData::getParam for the
    // derived-name (level/levtype) semantics of the second overload.
    template <typename T>
    std::conditional_t<std::is_same_v<T, atlas::Field>, FieldView, T> getParam(const std::string& name) const {
        if constexpr (std::is_same_v<T, atlas::Field>) {
            return FieldView(ModelData::getParam<atlas::Field>(name));
        }
        else {
            return ModelData::getParam<T>(name);
        }
    }

    template <typename T>
    std::conditional_t<std::is_same_v<T, atlas::Field>, FieldView, T> getParam(
        const std::string& name, const std::string& level, const std::string& levtype = "hl") const {
        if constexpr (std::is_same_v<T, atlas::Field>) {
            return FieldView(ModelData::getParam<atlas::Field>(name, level, levtype));
        }
        else {
            return ModelData::getParam<T>(name, level, levtype);
        }
    }

    // Write model values back within the authorised write-back protocol.
    using ModelData::writeParam;

    // Query updated state and availability.
    using ModelData::hasParameter;
    using ModelData::isUpdated;
    using ModelData::listAvailableParameters;

    // Diagnostics.
    using ModelData::print;

    // Identity of the plugin this view was filtered for (read-only).
    using ModelData::consumer;
};

}  // namespace data
}  // namespace plume
