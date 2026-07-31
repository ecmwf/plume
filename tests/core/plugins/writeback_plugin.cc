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
#include "writeback_plugin.h"
#include "plume/Protocol.h"

namespace plume_example_plugin {

REGISTER_LIBRARY(WriteBackPlugin)

WriteBackPlugin::WriteBackPlugin() : Plugin("WriteBackPlugin") {}

const WriteBackPlugin& WriteBackPlugin::instance() {
    static WriteBackPlugin instance;
    return instance;
}

plume::Protocol WriteBackPlugin::negotiate() {
    plume::Protocol protocol;
    protocol.require<int>("I");
    protocol.requireWritable<int>("W_INT");
    return protocol;
}

static plume::PluginCoreBuilder<WriteBackPluginCore> writeBackCoreBuilder_;

WriteBackPluginCore::WriteBackPluginCore(const eckit::Configuration& conf) : PluginCore(conf) {}

void WriteBackPluginCore::run() {
    modelData().writeParam<int>("W_INT", 42);
}

}  // namespace plume_example_plugin
