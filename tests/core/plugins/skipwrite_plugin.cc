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
#include "plume/Protocol.h"
#include "writeback_plugin.h"

namespace plume_example_plugin {

REGISTER_LIBRARY(SkipWritePlugin)

SkipWritePlugin::SkipWritePlugin() : Plugin("SkipWritePlugin") {}

const SkipWritePlugin& SkipWritePlugin::instance() {
    static SkipWritePlugin instance;
    return instance;
}

plume::Protocol SkipWritePlugin::negotiate() {
    plume::Protocol protocol;
    protocol.require<int>("I");
    protocol.requireWritable<int>("W_INT");
    return protocol;
}

static plume::PluginCoreBuilder<SkipWritePluginCore> skipWriteCoreBuilder_;

SkipWritePluginCore::SkipWritePluginCore(const eckit::Configuration& conf) : PluginCore(conf) {}

void SkipWritePluginCore::run() {
    if (++callCount_ % 2 == 1) {  // write on odd calls (1, 3, ...), skip on even (2, 4, ...)
        modelData().writeParam<int>("W_INT", callCount_ * 10);
    }
}

}  // namespace plume_example_plugin
