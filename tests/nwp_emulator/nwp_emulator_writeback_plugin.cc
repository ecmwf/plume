/*
 * (C) Copyright 2025- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 *
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */
#include "nwp_emulator_writeback_plugin.h"

#include "atlas/array/MakeView.h"

#include "nwp_emulator/nwp_definitions.h"

namespace nwp_emulator_test_plugin {

REGISTER_LIBRARY(NWPEmulatorWriteBackPlugin)

NWPEmulatorWriteBackPlugin::NWPEmulatorWriteBackPlugin() : Plugin("NWPEmulatorWriteBackPlugin"){};

NWPEmulatorWriteBackPlugin::~NWPEmulatorWriteBackPlugin(){};

const NWPEmulatorWriteBackPlugin& NWPEmulatorWriteBackPlugin::instance() {
    static NWPEmulatorWriteBackPlugin instance;
    return instance;
}
//--------------------------------------------------------------

// Sentinel value written into every point/level of "u" so tests can confirm the write reached the
// emulator's own field buffer.
static constexpr FIELD_TYPE_REAL kWriteBackSentinel = 12345.0;

// NWPEmulatorWriteBackPluginCore
static plume::PluginCoreBuilder<NWPEmulatorWriteBackPluginCore> runnable_plugincore_writeback_builder_;

NWPEmulatorWriteBackPluginCore::NWPEmulatorWriteBackPluginCore(const eckit::Configuration& conf) :
    PluginCore(conf) {}

NWPEmulatorWriteBackPluginCore::~NWPEmulatorWriteBackPluginCore() {}

void NWPEmulatorWriteBackPluginCore::run() {
    modelData().writeParam("u", [](plume::data::FieldWriter& f) {
        auto view              = atlas::array::make_view<FIELD_TYPE_REAL, 2>(f);
        const atlas::idx_t dim0 = view.shape(0);
        const atlas::idx_t dim1 = view.shape(1);
        for (atlas::idx_t i = 0; i < dim0; ++i) {
            for (atlas::idx_t j = 0; j < dim1; ++j) {
                view(i, j) = kWriteBackSentinel;
            }
        }
    });
    eckit::Log::info() << "NWPEmulatorWriteBackPlugin wrote back into 'u'" << std::endl;
}

//--------------------------------------------------------------

}  // namespace nwp_emulator_test_plugin
