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

// ===========================================================================================
// Example 4: hook points, and per-hook access to parameters (read-only / writable)
//
// A hook point is a named place of the model time step from which the model calls Plume. This
// toy model has two of them, around its "physics":
//
//      for each step:
//          Manager::run("pre-physics")     <- plugins bound to "pre-physics" run here
//          ... physics ...
//          Manager::run("post-physics")    <- plugins bound to "post-physics" run here
//
// It shares three parameters with the plugins. Each of them is offered at both hook points, with
// an access mode that depends on the hook point:
//
//      parameter    | pre-physics | post-physics
//      -------------+-------------+-------------
//      step         | read-only   | read-only
//      forcing      | writable    | read-only
//      temperature  | read-only   | writable
//
// What a plugin actually gets is the outcome of the negotiation between these offers and what
// the plugin requests (here through plume_config_hooks_writeback.yml, for the plugin in
// plugin_hooks.{h,cc}). A plugin is only granted write access to a parameter at a hook point if
// the model offers the parameter writable *at that hook point*.
//
// See README.md in this directory for a detailed description.
// ===========================================================================================

#include <iostream>
#include <string>

#include "eckit/config/YAMLConfiguration.h"
#include "eckit/runtime/Main.h"

#include "plume/Manager.h"
#include "plume/data/ModelData.h"


namespace {

// Ingest the parameters written back by the plugins during the last Manager::run(hook) call.
//
// Each Manager::run(hook) call is one write-back cycle: when it returns, the parameters that the
// plugins have written during that call are "pending". The model must acknowledge each of them,
// which tells Plume that the model has taken the new value on board. A parameter that is still
// pending cannot be written again: the next Manager::run(hook) call that opens it for writing
// would throw. Ingesting right after each Manager::run(hook) call is therefore the pattern to follow.
void ingestWritebacks(plume::data::ModelData& data, const std::string& hook) {
    for (const auto& name : data.pendingWritebacks()) {

        // This is where a model would copy the new value back into its own data structures.
        // Nothing to do in this example: the parameters are provided to Plume by pointer (see
        // provideParam() below), so the plugins have written directly into the model variables.
        std::cout << "[model] '" << name << "' was written back at hook point '" << hook << "'" << std::endl;

        data.acknowledgeWriteback(name);
    }
}

}  // namespace


int main(int argc, char** argv) {

    ASSERT_MSG(argc >= 2, "NOTE: this example is for illustration purposes only, error checks are missing!");

    std::cout << "*** Running Example 4 (C++ interface) ***" << std::endl;

    // init eckit main
    eckit::Main::initialise(argc, argv);

    // Plume manager configuration
    char* plumeConfigPath = argv[1];
    eckit::YAMLConfiguration plumeConfig(eckit::PathName{plumeConfigPath});
    plume::Manager::configure(plumeConfig);

    // ----------------------------------- Offers -----------------------------------
    plume::Protocol offers;

    // 1) Register the hook points that this model is able to call Plume from.
    //    The "default" hook point is always registered on top of these: it is the one that the
    //    argument-less Manager::run() targets, and the one that plugins declaring no hook point
    //    are bound to.
    offers.offerHook("pre-physics", "before the physics is computed");
    offers.offerHook("post-physics", "after the physics has been computed");

    // 2) Offer the parameters.
    //
    //    offer<T>() offers a parameter at every hook point, with the same access mode everywhere
    //    (read-only unless its optional "writable" argument is set to true).
    offers.offer<int>("step", "always", "current step of the model");

    //    offerAt<T>() offers a parameter at the listed hook points only. Offering the same
    //    parameter several times merges the definitions hook point by hook point, which is how a
    //    parameter is offered writable at some hook points and read-only at others.
    //
    //    "forcing" may be set by plugins before the physics; afterwards it can only be read.
    offers.offerAt<double>("forcing", "always", "forcing applied by the physics", {"pre-physics"}, /*writable=*/true);
    offers.offerAt<double>("forcing", "always", "forcing applied by the physics", {"post-physics"});

    //    "temperature" can only be read before the physics; plugins may correct it afterwards.
    offers.offerAt<double>("temperature", "always", "temperature of the model", {"pre-physics"});
    offers.offerAt<double>("temperature", "always", "temperature of the model", {"post-physics"}, /*writable=*/true);

    //    Note: a parameter that is not offered at a hook point at all (e.g. offered with
    //    offerAt<T>(..., {"post-physics"}) only) cannot be requested there, not even read-only:
    //    a plugin needing it at "pre-physics" would be rejected.

    // ----------------------------------- Negotiation -----------------------------------
    // Each plugin is accepted or rejected as a whole. With hook points, a plugin is accepted if:
    //  - all the hook points it asks for are registered by the model;
    //  - each parameter it requests is offered at every hook point it needs it at;
    //  - wherever it requests a parameter writable, the model offers it writable at that hook
    //    point (and the write-back policy of the configuration is not "disabled").
    plume::Manager::negotiate(offers);

    // After negotiation the model can query the outcome, hook point by hook point:
    //  - isHookActive(): is any plugin bound to this hook point? (if not, the model can skip
    //    whatever it does only to prepare that call);
    //  - getActiveParamsAtHook(): parameters that the plugins use at this hook point, i.e. what
    //    the model has to keep up to date before calling it;
    //  - getWritableParamsAtHook(): parameters that the plugins may write at this hook point,
    //    i.e. what the model may have to ingest after calling it.
    for (const auto& hook : plume::Manager::registeredHooks()) {
        std::cout << "[model] hook point '" << hook << "' is "
                  << (plume::Manager::isHookActive(hook) ? "active" : "not used by any plugin");
        std::cout << "; params used: [ ";
        for (const auto& param : plume::Manager::getActiveParamsAtHook(hook)) {
            std::cout << param << " ";
        }
        std::cout << "]; params that may be written back: [ ";
        for (const auto& param : plume::Manager::getWritableParamsAtHook(hook)) {
            std::cout << param << " ";
        }
        std::cout << "]" << std::endl;
    }

    // ----------------------------------- Data -----------------------------------
    // Model variables. They are provided to Plume by pointer: plugins read them - and, where they
    // have been granted write access, write them - directly in the memory of the model.
    int step           = 0;
    double forcing     = 0.0;
    double temperature = 280.0;

    plume::data::ModelData data;
    data.provideParam("step", &step);
    data.provideParam("forcing", &forcing);
    data.provideParam("temperature", &temperature);

    // Feed plugins with the data
    plume::Manager::feedPlugins(data);

    // ----------------------------------- Run -----------------------------------
    for (step = 1; step <= 5; step++) {

        // --- hook point "pre-physics" ---
        // Only "forcing" is open for writing during this call: a plugin trying to write
        // "temperature" here would get an exception.
        plume::Manager::run("pre-physics");
        ingestWritebacks(data, "pre-physics");

        // the "physics" of this model, which uses the forcing set by the plugin
        temperature += forcing;
        std::cout << "[model] step " << step << ": physics done, temperature=" << temperature << std::endl;

        // --- the default hook point ---
        // No plugin of this example is bound to it, but a model that adopts hook points should
        // keep this call: plugins that declare no hook point (e.g. all the plugins written before
        // hook points existed) only run here.
        plume::Manager::run();

        // --- hook point "post-physics" ---
        // The access modes are now swapped: "temperature" is open for writing, "forcing" is not.
        plume::Manager::run("post-physics");
        ingestWritebacks(data, "post-physics");

        std::cout << "[model] step " << step << ": end of step, temperature=" << temperature << std::endl;
    }

    // Teardown as necessary
    plume::Manager::teardown();

    // terminate eckit main
    eckit::Main::instance().terminate();

    std::cout << "*** Example 4 complete. ***" << std::endl;
    std::cout << std::endl;
}
