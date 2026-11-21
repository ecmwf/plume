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
#include <string>

#include "plume/Plugin.h"
#include "plume/PluginCore.h"

namespace plugin_hooks {


/**
 * @brief PluginCore used by example 4 (hook points and per-hook access to parameters).
 *
 * It is run at two hook points of the model ("pre-physics" and "post-physics") and behaves
 * differently at each of them: see run() in plugin_hooks.cc.
 */
class PluginCoreHooks final : public plume::PluginCore {
public:
    PluginCoreHooks(const eckit::Configuration& conf);
    void run() override;
    constexpr static const char* type() { return "plugincore-hooks"; }

private:
    // log whether a param is writable or read-only at the hook point being run
    void logAccess(const std::string& name);
};


/**
 * @brief Plugin used by example 4 (hook points and per-hook access to parameters).
 */
class PluginHooks final : public plume::Plugin {

public:
    PluginHooks();

    /**
     * This plugin deliberately declares nothing here: the hook points it runs at, the parameters
     * it needs at each of them and their access mode (read-only or writable) all come from its
     * entry in the Plume configuration (see plume_config_hooks_writeback.yml).
     *
     * The same request could be made from code instead - the two can also be combined, in which
     * case the definitions of a parameter are merged hook point by hook point ("writable wins"):
     *
     *   protocol.requireHook("pre-physics");
     *   protocol.requireHook("post-physics");
     *
     *   // read-only at both hook points (a plain require<int>("step") would be equivalent,
     *   // as a parameter with no hook scope is needed at every hook point of the plugin)
     *   protocol.requireAt<int>("step", {"pre-physics", "post-physics"});
     *
     *   // written at "pre-physics", read-only at "post-physics"
     *   protocol.requireWritableAt<double>("forcing", {"pre-physics"});
     *   protocol.requireAt<double>("forcing", {"post-physics"});
     *
     *   // read-only at "pre-physics", written at "post-physics"
     *   protocol.requireAt<double>("temperature", {"pre-physics"});
     *   protocol.requireWritableAt<double>("temperature", {"post-physics"});
     *
     * Note that hook points listed in the configuration ("hooks" key of the plugin) *replace*
     * the ones required here, so that a deployment can re-target a plugin without recompiling it.
     */
    plume::Protocol negotiate() override {
        plume::Protocol protocol;
        return protocol;
    }

    static const PluginHooks& instance();

    std::string version() const override { return "0.0.1"; }

    std::string gitsha1(unsigned int count) const override { return "undefined"; }

    virtual std::string plugincoreName() const override { return PluginCoreHooks::type(); }
};


}  // namespace plugin_hooks
