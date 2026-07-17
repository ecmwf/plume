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
#include "plugin_writeback_api.h"
#include "plume/Protocol.h"

namespace plume_writeback_test_api {

REGISTER_LIBRARY(WriteBackTestAPI)

WriteBackTestAPI::WriteBackTestAPI() : Plugin("WriteBackTestAPI") {}

const WriteBackTestAPI& WriteBackTestAPI::instance() {
    static WriteBackTestAPI instance;
    return instance;
}

plume::Protocol WriteBackTestAPI::negotiate() {
    plume::Protocol protocol;
    protocol.requireWritable<int>("W_INT");
    protocol.requireWritable<bool>("W_BOOL");
    protocol.requireWritable<float>("W_FLOAT");
    protocol.requireWritable<double>("W_DOUBLE");
    return protocol;
}

static plume::PluginCoreBuilder<WriteBackTestAPICore> writeBackTestAPICoreBuilder_;

WriteBackTestAPICore::WriteBackTestAPICore(const eckit::Configuration& conf) : PluginCore(conf) {}

void WriteBackTestAPICore::run() {
    modelData().writeParam<int>("W_INT", 42);
    modelData().writeParam<bool>("W_BOOL", true);
    modelData().writeParam<float>("W_FLOAT", 3.14f);
    modelData().writeParam<double>("W_DOUBLE", 2.718);
}

}  // namespace plume_writeback_test_api
