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

#include <string>

#include "plume/Plugin.h"
#include "plume/PluginCore.h"

/**
 * Fixture plugins for the hook point tests.
 *
 * Each plugincore records the fact that it has run by incrementing a Plume-owned counter
 * parameter. The counters are created by the test through ModelData::createParam(), so the
 * plugincore and the test share the same underlying value and the test can read back exactly
 * which plugin ran at which hook point.
 *
 * Note that REGISTER_LIBRARY() declares a fixed "libregist" symbol, so each plugin has to live in
 * its own translation unit.
 */
namespace plume_hook_plugin {

/**
 * @brief Increments a counter parameter, if the plugincore has been given one.
 */
void bumpCounter(plume::data::ModelData& data, const std::string& name);


// ---------- Bound to "hook-A" only ----------
class HookPluginCoreA final : public plume::PluginCore {
public:
    HookPluginCoreA(const eckit::Configuration& conf);
    void run() override;
    constexpr static const char* type() { return "hook-plugincore-a"; }
};

class HookPluginA final : public plume::Plugin {
public:
    HookPluginA();

    plume::Protocol negotiate() override {
        plume::Protocol protocol;
        protocol.require<int>("I");
        protocol.require<int>("count-A");
        protocol.requireHook("hook-A");
        return protocol;
    }

    static const HookPluginA& instance();

    std::string version() const override { return "0.0.1-HookA"; }
    std::string gitsha1(unsigned int count) const override { return "undefined"; }
    std::string plugincoreName() const override { return HookPluginCoreA::type(); }
};


// ---------- Bound to both "hook-A" and "hook-B" ----------
class HookPluginCoreAB final : public plume::PluginCore {
public:
    HookPluginCoreAB(const eckit::Configuration& conf);
    void run() override;
    constexpr static const char* type() { return "hook-plugincore-ab"; }
};

class HookPluginAB final : public plume::Plugin {
public:
    HookPluginAB();

    plume::Protocol negotiate() override {
        plume::Protocol protocol;
        protocol.require<int>("I");
        protocol.require<int>("count-AB-hook-A");
        protocol.require<int>("count-AB-hook-B");
        protocol.requireHook("hook-A");
        protocol.requireHook("hook-B");
        return protocol;
    }

    static const HookPluginAB& instance();

    std::string version() const override { return "0.0.1-HookAB"; }
    std::string gitsha1(unsigned int count) const override { return "undefined"; }
    std::string plugincoreName() const override { return HookPluginCoreAB::type(); }
};


// ---------- Declares no hook point: bound to the default one ----------
class HookPluginCoreDefault final : public plume::PluginCore {
public:
    HookPluginCoreDefault(const eckit::Configuration& conf);
    void run() override;
    constexpr static const char* type() { return "hook-plugincore-default"; }
};

class HookPluginDefault final : public plume::Plugin {
public:
    HookPluginDefault();

    plume::Protocol negotiate() override {
        plume::Protocol protocol;
        protocol.require<int>("J");
        protocol.require<int>("count-D");
        // deliberately no requireHook() call: this plugin must end up bound to plume::DEFAULT_HOOK
        return protocol;
    }

    static const HookPluginDefault& instance();

    std::string version() const override { return "0.0.1-HookDefault"; }
    std::string gitsha1(unsigned int count) const override { return "undefined"; }
    std::string plugincoreName() const override { return HookPluginCoreDefault::type(); }
};

}  // namespace plume_hook_plugin
