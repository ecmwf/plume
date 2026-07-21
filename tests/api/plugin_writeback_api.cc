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

#include "atlas/array/ArrayShape.h"
#include "atlas/array/ArrayView.h"
#include "atlas/array/DataType.h"
#include "atlas/array/MakeView.h"
#include "atlas/field/Field.h"

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
    protocol.requireWritable<atlas::Field>("W_FIELD");
    return protocol;
}

static plume::PluginCoreBuilder<WriteBackTestAPICore> writeBackTestAPICoreBuilder_;

WriteBackTestAPICore::WriteBackTestAPICore(const eckit::Configuration& conf) : PluginCore(conf) {}

void WriteBackTestAPICore::run() {
    modelData().writeParam<int>("W_INT", 42);
    modelData().writeParam<bool>("W_BOOL", true);
    modelData().writeParam<float>("W_FLOAT", 3.14f);
    modelData().writeParam<double>("W_DOUBLE", 2.718);

    // Build a brand-new field (distinct implementation from the model's) carrying the updated values, then write
    // it back. write-back must copy the data into the model's own buffer in place, not rebind the handle.
    const atlas::Field current = modelData().getParam<atlas::Field>("W_FIELD");
    atlas::Field update("W_FIELD", current.datatype(), current.shape());
    auto view = atlas::array::make_view<int, 1>(update);
    for (int i = 0; i < static_cast<int>(view.shape(0)); ++i) {
        view(i) = (i + 1) * 100;
    }
    modelData().writeParam<atlas::Field>("W_FIELD", update);
}

}  // namespace plume_writeback_test_api
