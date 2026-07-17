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

namespace plume_writeback_test_api {

class WriteBackTestAPICore final : public plume::PluginCore {
public:
    explicit WriteBackTestAPICore(const eckit::Configuration& conf);
    void run() override;
    static const char* type() { return "writeback-test-api-core"; }
};

class WriteBackTestAPI final : public plume::Plugin {
public:
    WriteBackTestAPI();

    plume::Protocol negotiate() override;

    static const WriteBackTestAPI& instance();

    std::string version() const override { return "0.0.1"; }
    std::string gitsha1(unsigned int) const override { return "N/A"; }
    std::string plugincoreName() const override { return WriteBackTestAPICore::type(); }
};

}  // namespace plume_writeback_test_api
