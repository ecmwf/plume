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
#include "plugin_writeback_fapi.h"

namespace plume_writeback_fapi {

REGISTER_LIBRARY(WriteBackFAPI)

WriteBackFAPI::WriteBackFAPI() : Plugin(WriteBackFAPI::name()) {}

const WriteBackFAPI& WriteBackFAPI::instance() {
    static WriteBackFAPI instance;
    return instance;
}

static plume::PluginCoreBuilder<WriteBackFAPICore> writeBackFAPICoreBuilder_;

WriteBackFAPICore::WriteBackFAPICore(const eckit::Configuration& conf) : PluginCore(conf), config_{conf} {}

}  // namespace plume_writeback_fapi
