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

namespace plume_example_plugin {

// ---------------------------------------------------------------------------
// WriteBackPluginCore — writes a fixed value to W_INT every run() call.
// ---------------------------------------------------------------------------
class WriteBackPluginCore final : public plume::PluginCore {
public:
    explicit WriteBackPluginCore(const eckit::Configuration& conf);
    void run() override;
    constexpr static const char* type() { return "writeback-plugincore"; }
};

// ---------------------------------------------------------------------------
// WriteBackPlugin — negotiates W_INT as writable.
// ---------------------------------------------------------------------------
class WriteBackPlugin final : public plume::Plugin {
public:
    WriteBackPlugin();

    plume::Protocol negotiate() override;

    static const WriteBackPlugin& instance();

    std::string version() const override { return "0.0.1-WriteBack"; }
    std::string gitsha1(unsigned int) const override { return "undefined"; }
    std::string plugincoreName() const override { return WriteBackPluginCore::type(); }
};

// ---------------------------------------------------------------------------
// SkipWritePluginCore — only writes W_INT on odd-numbered calls (simulates
// a plugin that conditionally skips writing in some cycles).
// ---------------------------------------------------------------------------
class SkipWritePluginCore final : public plume::PluginCore {
public:
    explicit SkipWritePluginCore(const eckit::Configuration& conf);
    void run() override;
    constexpr static const char* type() { return "skipwrite-plugincore"; }

private:
    int callCount_{0};
};

// ---------------------------------------------------------------------------
// SkipWritePlugin — negotiates W_INT as writable, but only writes every
// other cycle.
// ---------------------------------------------------------------------------
class SkipWritePlugin final : public plume::Plugin {
public:
    SkipWritePlugin();

    plume::Protocol negotiate() override;

    static const SkipWritePlugin& instance();

    std::string version() const override { return "0.0.1-SkipWrite"; }
    std::string gitsha1(unsigned int) const override { return "undefined"; }
    std::string plugincoreName() const override { return SkipWritePluginCore::type(); }
};

}  // namespace plume_example_plugin
