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

#include "plume/Plugin.h"
#include "plume/PluginCore.h"

extern "C" void plugincore_setup_writeback_fapi(void* conf, void* modelData);
extern "C" void plugincore_run_writeback_fapi();
extern "C" void plugincore_teardown_writeback_fapi();

namespace plume_writeback_fapi {

class WriteBackFAPICore final : public plume::PluginCore {
public:
    explicit WriteBackFAPICore(const eckit::Configuration& conf);

    void setup() override { plugincore_setup_writeback_fapi(&config_, &modelData()); }
    void run() override { plugincore_run_writeback_fapi(); }
    void teardown() override { plugincore_teardown_writeback_fapi(); }

    static const char* type() { return "writeback-fapi-core"; }

private:
    eckit::LocalConfiguration config_;
};

class WriteBackFAPI final : public plume::Plugin {
public:
    WriteBackFAPI();

    plume::Protocol negotiate() override { return plume::Protocol{}; }

    static const WriteBackFAPI& instance();
    static const char* name() { return "WriteBackTestFAPI"; }

    std::string version() const override { return "0.0.1"; }
    std::string gitsha1(unsigned int) const override { return "N/A"; }
    std::string plugincoreName() const override { return WriteBackFAPICore::type(); }
};

}  // namespace plume_writeback_fapi
