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
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "eckit/config/YAMLConfiguration.h"
#include "eckit/testing/Test.h"

#include "plume/Hook.h"
#include "plume/Negotiator.h"
#include "plume/coupling/WriteAuthorisation.h"
#include "plume/coupling/WriteBackPolicy.h"
#include "plume/data/ParameterCatalogue.h"


using namespace eckit::testing;
using namespace plume;

namespace plume::test {

// =================================================================================================
// Test helpers
// =================================================================================================

// Fluent builder for single-key YAML parameter lists (offered: / required:).
// Usage:
//   ParamListBuilder("offered").add("I", "INT").add("J", "INT", true)
//   ParamListBuilder("required").add("I", "INT")
struct ParamListBuilder {
    explicit ParamListBuilder(const std::string& key) : yaml_{key + ":\n"} {}

    ParamListBuilder& add(const std::string& name, const std::string& type, bool writable = false) {
        yaml_ += "  - name: " + name + "\n";
        yaml_ += "    type: " + type + "\n";
        yaml_ += "    available: always\n";
        if (writable)
            yaml_ += "    writable: true\n";
        return *this;
    }

    // Implicit conversion so the builder can be passed wherever std::string is expected.
    operator std::string() const { return yaml_; }

    // Convenience: build an eckit::YAMLConfiguration directly.
    eckit::YAMLConfiguration cfg() const { return eckit::YAMLConfiguration(yaml_); }

private:
    std::string yaml_;
};

// Fluent builder for the "parameters:" YAML block used in manager config_params.
// Each call to group(...) adds one parameter group (list of params).
// Usage:
//   ConfigGroupBuilder().group({{"I","INT",true}}).group({{"J","INT",false}})
struct ParamEntry {
    std::string name, type;
    bool writable = false;
};

struct ConfigGroupBuilder {
    std::string yaml_{"parameters:\n"};

    ConfigGroupBuilder& group(std::initializer_list<ParamEntry> params) {
        yaml_ += "  -\n";
        for (const auto& p : params) {
            yaml_ += "    - name: " + p.name + "\n";
            yaml_ += "      type: " + p.type + "\n";
            if (p.writable)
                yaml_ += "      writable: true\n";
        }
        return *this;
    }

    // Parse and return the sub-configurations ready to pass to Negotiator::negotiate().
    std::vector<eckit::LocalConfiguration> parse() const {
        return eckit::YAMLConfiguration(yaml_).getSubConfigurations("parameters");
    }
};

// Return the writable flag for a named param in a PluginDecision, or nullopt if not found.
static std::optional<bool> decisionParamWritable(const PluginDecision& decision, const std::string& name) {
    for (const auto& param : decision.offeredParams()) {
        if (param.name() == name)
            return param.writable();
    }
    return std::nullopt;
}

// Shared offers: I writable, J read-only — used across writable and authorisation tests.
static const std::string kOffersIWritableJRO = ParamListBuilder("offered").add("I", "INT", true).add("J", "INT");

// =================================================================================================
// Tests
// =================================================================================================

CASE("test_negotiator - basic accept / reject / invalid-key") {

    Negotiator negotiator;

    auto offers = ParamListBuilder("offered").add("I", "INT").add("J", "INT").add("K", "INT");
    auto
        requires
    = ParamListBuilder("required").add("I", "INT").add("J", "INT").add("K", "INT");
    auto requires_not_fulfilled = ParamListBuilder("required")
                                      .add("I", "INT")
                                      .add("J", "INT")
                                      .add("K", "INT")
                                      .add("K_new", "INT")
                                      .add("K_new2", "INT");

    // Manually-built invalid-key YAML (cannot be expressed via the builder).
    const std::string requires_invalid_str = R"YAML(
    required_invalid_key:
      - name: I
        type: INT
        available: always
    )YAML";

    EXPECT_EQUAL(negotiator.negotiate("TestPlugin", offers.cfg(), requires.cfg()).accepted(), true);
    EXPECT_EQUAL(negotiator.negotiate("TestPlugin", offers.cfg(), requires_not_fulfilled.cfg()).accepted(), false);
    EXPECT_THROWS(negotiator.negotiate("TestPlugin", offers.cfg(), eckit::YAMLConfiguration(requires_invalid_str)));
}


CASE("test_negotiator - writable params") {
    // Write-back must be explicitly enabled; single_writer tests the offer-side gate.
    Negotiator negotiator(WriteBackPolicy::single_writer);

    auto offers = ParamListBuilder("offered").add("I", "INT", true).add("J", "INT");

    // I offered as writable -> writable request accepted
    auto requires_writable_ok = ParamListBuilder("required").add("I", "INT", true);
    EXPECT_EQUAL(negotiator.negotiate("PluginA", offers.cfg(), requires_writable_ok.cfg()).accepted(), true);

    // J not offered as writable -> writable request rejected
    auto requires_writable_rejected = ParamListBuilder("required").add("J", "INT", true);
    EXPECT_EQUAL(negotiator.negotiate("PluginA", offers.cfg(), requires_writable_rejected.cfg()).accepted(), false);

    // I offered as writable, but plugin requests read-only -> accepted
    auto requires_readonly = ParamListBuilder("required").add("I", "INT");
    EXPECT_EQUAL(negotiator.negotiate("PluginA", offers.cfg(), requires_readonly.cfg()).accepted(), true);
}


CASE("test_negotiator - write-back disabled by default") {
    Negotiator negotiator;  // policy = disabled

    auto offers       = ParamListBuilder("offered").add("I", "INT", true);
    auto req_writable = ParamListBuilder("required").add("I", "INT", true);
    auto req_ro       = ParamListBuilder("required").add("I", "INT");

    // Disabled policy rejects writable requests even when the model offers them.
    EXPECT_EQUAL(negotiator.negotiate("TestPlugin", offers.cfg(), req_writable.cfg()).accepted(), false);
    // Read-only is unaffected by write-back policy.
    EXPECT_EQUAL(negotiator.negotiate("TestPlugin", offers.cfg(), req_ro.cfg()).accepted(), true);
}


CASE("test_negotiator - read-only code require + writable config request upgrades to writable") {
    // ParameterDefinition::operator< compares by name only, so std::set::insert silently
    // drops a writable entry if a read-only entry with the same name already exists.
    
    // Scenario (edge): plugin code requires I read-only; config group requests I writable.
    // The final PluginDecision must mark I as writable.
    // The paramMap + insertOrUpgrade ensure writable wins regardless of insertion order.
    Negotiator negotiator(WriteBackPolicy::single_writer);

    auto offers = ParamListBuilder("offered").add("I", "INT", true);
    auto req_ro = ParamListBuilder("required").add("I", "INT");
    auto cfg_w  = ConfigGroupBuilder().group({{"I", "INT", true}}).parse();

    PluginDecision decision = negotiator.negotiate("PluginA", offers.cfg(), req_ro.cfg(), cfg_w);
    EXPECT_EQUAL(decision.accepted(), true);

    // I must be writable — config group's writable request must not be lost to insertion order.
    auto writable = decisionParamWritable(decision, "I");
    EXPECT(writable.has_value());
    EXPECT_EQUAL(*writable, true);
}


CASE("test_negotiator - writable code require + read-only config request stays writable") {
    // Complementary direction: code=writable, config=RO -> writable must be preserved.
    Negotiator negotiator(WriteBackPolicy::single_writer);

    auto offers = ParamListBuilder("offered").add("I", "INT", true);
    auto req_w  = ParamListBuilder("required").add("I", "INT", true);
    auto cfg_ro = ConfigGroupBuilder().group({{"I", "INT"}}).parse();

    PluginDecision decision = negotiator.negotiate("PluginA", offers.cfg(), req_w.cfg(), cfg_ro);
    EXPECT_EQUAL(decision.accepted(), true);

    auto writable = decisionParamWritable(decision, "I");
    EXPECT(writable.has_value());
    EXPECT_EQUAL(*writable, true);
}


CASE("test_negotiator - derived param cannot be writable") {
    // Requesting a derived param as writable must throw at ParameterDefinition construction.
    eckit::YAMLConfiguration derived_writable_requires(std::string(R"YAML(
    required:
      - name: u
        type: ATLAS_FIELD
        height: 100
        writable: true
    )YAML"));

    EXPECT_THROWS(Protocol bad_protocol(derived_writable_requires));
}


// =================================================================================================
// Write authorisation tests
// =================================================================================================

CASE("test_negotiator - write authorisation: pure read-only plugin is not authorised to write") {
    // A plugin requesting I read-only must NOT appear in write authorisation,
    // even though I is offered as writable by the model.
    Negotiator negotiator(WriteBackPolicy::single_writer);

    auto req_ro   = ParamListBuilder("required").add("I", "INT");
    auto decision = negotiator.negotiate("PluginRO", eckit::YAMLConfiguration(kOffersIWritableJRO), req_ro.cfg());
    EXPECT_EQUAL(decision.accepted(), true);

    WriteAuthorisation auth = negotiator.writeAuthorisation();
    EXPECT_EQUAL(auth.isAuthorised("PluginRO", "I"), false);
    EXPECT(auth.empty());
}


CASE("test_negotiator - write authorisation: writable plugin is authorised") {
    // A plugin requesting I as writable must appear in write authorisation.
    Negotiator negotiator(WriteBackPolicy::single_writer);

    auto req_w    = ParamListBuilder("required").add("I", "INT", true);
    auto decision = negotiator.negotiate("PluginW", eckit::YAMLConfiguration(kOffersIWritableJRO), req_w.cfg());
    EXPECT_EQUAL(decision.accepted(), true);

    WriteAuthorisation auth = negotiator.writeAuthorisation();
    EXPECT_EQUAL(auth.isAuthorised("PluginW", "I"), true);
    EXPECT_EQUAL(auth.isAuthorised("PluginW", "J"), false);  // J was not requested

    auto plugins = auth.authorisedPlugins();
    EXPECT_EQUAL(plugins.size(), 1u);
    EXPECT(plugins.count("PluginW") == 1);
}


CASE("test_negotiator - write authorisation: RO+RO combo does not grant write access") {
    // Critical safety test: code=RO, config=RO -> writable-wins merge must NOT promote to writable.
    Negotiator negotiator(WriteBackPolicy::single_writer);

    auto req_ro = ParamListBuilder("required").add("I", "INT");
    auto cfg_ro = ConfigGroupBuilder().group({{"I", "INT"}}).parse();

    auto decision =
        negotiator.negotiate("PluginROx2", eckit::YAMLConfiguration(kOffersIWritableJRO), req_ro.cfg(), cfg_ro);
    EXPECT_EQUAL(decision.accepted(), true);

    WriteAuthorisation auth = negotiator.writeAuthorisation();
    EXPECT_EQUAL(auth.isAuthorised("PluginROx2", "I"), false);
    EXPECT(auth.empty());
}


CASE("test_negotiator - write authorisation: writable-wins RO+writable grants write access") {
    // code=RO, config=writable -> writable-wins upgrade is reflected in authorisation.
    Negotiator negotiator(WriteBackPolicy::single_writer);

    auto req_ro = ParamListBuilder("required").add("I", "INT");
    auto cfg_w  = ConfigGroupBuilder().group({{"I", "INT", true}}).parse();

    auto decision =
        negotiator.negotiate("PluginUpgraded", eckit::YAMLConfiguration(kOffersIWritableJRO), req_ro.cfg(), cfg_w);
    EXPECT_EQUAL(decision.accepted(), true);

    WriteAuthorisation auth = negotiator.writeAuthorisation();
    EXPECT_EQUAL(auth.isAuthorised("PluginUpgraded", "I"), true);
}


CASE("test_negotiator - write authorisation: only the writing plugin is authorised across two plugins") {
    // Plugin A writes I, Plugin B reads I -> under multi_writer both accepted; only A authorised.
    Negotiator negotiator(WriteBackPolicy::multi_writer);

    auto req_w  = ParamListBuilder("required").add("I", "INT", true);
    auto req_ro = ParamListBuilder("required").add("I", "INT");
    auto offers = eckit::YAMLConfiguration(kOffersIWritableJRO);

    EXPECT_EQUAL(negotiator.negotiate("PluginA", offers, req_w.cfg()).accepted(), true);
    EXPECT_EQUAL(negotiator.negotiate("PluginB", offers, req_ro.cfg()).accepted(), true);

    WriteAuthorisation auth = negotiator.writeAuthorisation();
    EXPECT_EQUAL(auth.isAuthorised("PluginA", "I"), true);
    EXPECT_EQUAL(auth.isAuthorised("PluginB", "I"), false);

    auto plugins = auth.authorisedPlugins();
    EXPECT_EQUAL(plugins.size(), 1u);
    EXPECT(plugins.count("PluginA") == 1);
}


CASE("test_negotiator_hooks") {

    Negotiator negotiator;

    Protocol offers;
    offers.offer<int>("I", "always", "none-1");
    offers.offerHook("hook-A", "the first hook point");

    // 1) a plugin that requires no hook point is bound to the default one
    Protocol requiresNoHook;
    requiresNoHook.require<int>("I");
    PluginDecision decisionNoHook = negotiator.negotiate("TestPlugin", offers, requiresNoHook);
    EXPECT_EQUAL(decisionNoHook.accepted(), true);
    EXPECT_EQUAL(decisionNoHook.agreedHooks(), std::set<std::string>{DEFAULT_HOOK});

    // 2) a plugin that requires an offered hook point is bound to it, and to it only
    Protocol requiresA;
    requiresA.require<int>("I");
    requiresA.requireHook("hook-A");
    PluginDecision decisionA = negotiator.negotiate("TestPlugin", offers, requiresA);
    EXPECT_EQUAL(decisionA.accepted(), true);
    EXPECT_EQUAL(decisionA.agreedHooks(), std::set<std::string>{"hook-A"});

    // 3) hook requirements are all-or-nothing: "hook-B" is not offered, so the plugin is rejected
    //    even though "hook-A" is
    Protocol requiresAB;
    requiresAB.require<int>("I");
    requiresAB.requireHook("hook-A");
    requiresAB.requireHook("hook-B");
    PluginDecision decisionAB = negotiator.negotiate("TestPlugin", offers, requiresAB);
    EXPECT_EQUAL(decisionAB.accepted(), false);

    // 4) the default hook point is always offered, even by a model that registers none
    Protocol bareOffers;
    bareOffers.offer<int>("I", "always", "none-1");
    Protocol requiresDefault;
    requiresDefault.require<int>("I");
    requiresDefault.requireHook(DEFAULT_HOOK);
    EXPECT_EQUAL(negotiator.negotiate("TestPlugin", bareOffers, requiresDefault).accepted(), true);
}


CASE("test_negotiator_hooks_from_config") {

    Negotiator negotiator;

    Protocol offers;
    offers.offer<int>("I", "always", "none-1");
    offers.offerHook("hook-A", "the first hook point");
    offers.offerHook("hook-B", "the second hook point");

    Protocol requiresA;
    requiresA.require<int>("I");
    requiresA.requireHook("hook-A");

    // narrowing/re-targeting: the configured hook point replaces the declared one
    std::optional<std::set<std::string>> configB = std::set<std::string>{"hook-B"};
    PluginDecision decisionB = negotiator.negotiate("TestPlugin", offers, requiresA, {}, configB);
    EXPECT_EQUAL(decisionB.accepted(), true);
    EXPECT_EQUAL(decisionB.agreedHooks(), std::set<std::string>{"hook-B"});

    // widening: a plugin that declares nothing can be bound to several hook points by config
    Protocol requiresNoHook;
    requiresNoHook.require<int>("I");
    std::optional<std::set<std::string>> configAB = std::set<std::string>{"hook-A", "hook-B"};
    PluginDecision decisionAB = negotiator.negotiate("TestPlugin", offers, requiresNoHook, {}, configAB);
    EXPECT_EQUAL(decisionAB.accepted(), true);
    std::set<std::string> expectedAB = {"hook-A", "hook-B"};
    EXPECT_EQUAL(decisionAB.agreedHooks(), expectedAB);

    // a configured hook point that the model does not offer rejects the plugin
    std::optional<std::set<std::string>> configMissing = std::set<std::string>{"hook-C"};
    EXPECT_EQUAL(negotiator.negotiate("TestPlugin", offers, requiresA, {}, configMissing).accepted(), false);
}


CASE("test_negotiator_hook_scoped_params") {

    Negotiator negotiator;

    // the model keeps u and v current everywhere, pz0m only before the surface exchange and swh
    // only at the wave model hook point
    Protocol offers;
    offers.offerHook("before-surface", "before the surface exchange");
    offers.offerHook("after-phys", "after the physics");
    offers.offerHook("wam", "wave model");
    offers.offer<int>("u", "always", "u");
    offers.offer<int>("v", "always", "v");
    offers.offerAt<int>("pz0m", "always", "pz0m", {"before-surface"});
    offers.offerAt<int>("swh", "always", "swh", {"wam"});

    const std::set<std::string> beforeSurfaceAndAfterPhys = {"before-surface", "after-phys"};

    // 1) u, v needed at both hook points, pz0m only before the surface exchange
    Protocol requiresWF;
    requiresWF.requireHook("before-surface");
    requiresWF.requireHook("after-phys");
    requiresWF.require<int>("u");
    requiresWF.require<int>("v");
    requiresWF.requireAt<int>("pz0m", {"before-surface"});
    PluginDecision decisionWF = negotiator.negotiate("TestPlugin", offers, requiresWF);
    EXPECT_EQUAL(decisionWF.accepted(), true);
    EXPECT_EQUAL(decisionWF.agreedHooks(), beforeSurfaceAndAfterPhys);
    std::set<std::string> expectedBefore = {"pz0m", "u", "v"};
    std::set<std::string> expectedAfter  = {"u", "v"};
    EXPECT_EQUAL(decisionWF.offeredParamNamesAtHook("before-surface"), expectedBefore);
    EXPECT_EQUAL(decisionWF.offeredParamNamesAtHook("after-phys"), expectedAfter);
    // the plugin still gets all its params (data feeding is not per hook)
    EXPECT_EQUAL(decisionWF.offeredParamNames(), expectedBefore);

    // 2) pz0m needed at a hook point where the model does not offer it: rejected
    Protocol requiresPz0mAfter;
    requiresPz0mAfter.requireHook("after-phys");
    requiresPz0mAfter.require<int>("pz0m");
    EXPECT_EQUAL(negotiator.negotiate("TestPlugin", offers, requiresPz0mAfter).accepted(), false);

    // 3) swh required at "wam", offered there: accepted; offered elsewhere only: rejected
    Protocol requiresSwh;
    requiresSwh.requireHook("wam");
    requiresSwh.requireHook("after-phys");
    requiresSwh.requireAt<int>("swh", {"wam"});
    EXPECT_EQUAL(negotiator.negotiate("TestPlugin", offers, requiresSwh).accepted(), true);

    Protocol requiresSwhAfter;
    requiresSwhAfter.requireHook("after-phys");
    requiresSwhAfter.requireAt<int>("swh", {"after-phys"});
    EXPECT_EQUAL(negotiator.negotiate("TestPlugin", offers, requiresSwhAfter).accepted(), false);

    // 4) a scope naming a hook point the plugin is not bound to: rejected
    Protocol requiresUnbound;
    requiresUnbound.requireHook("after-phys");
    requiresUnbound.requireAt<int>("u", {"before-surface"});
    EXPECT_EQUAL(negotiator.negotiate("TestPlugin", offers, requiresUnbound).accepted(), false);

    // ... including when a configured "hooks" override takes that hook point away
    std::optional<std::set<std::string>> configAfterOnly = std::set<std::string>{"after-phys"};
    EXPECT_EQUAL(negotiator.negotiate("TestPlugin", offers, requiresWF, {}, configAfterOnly).accepted(), false);

    // 5) requiring the same param at two hook points unites the scopes
    Protocol requiresTwice;
    requiresTwice.requireHook("before-surface");
    requiresTwice.requireHook("after-phys");
    requiresTwice.requireAt<int>("u", {"before-surface"});
    requiresTwice.requireAt<int>("u", {"after-phys"});
    PluginDecision decisionTwice = negotiator.negotiate("TestPlugin", offers, requiresTwice);
    EXPECT_EQUAL(decisionTwice.accepted(), true);
    EXPECT_EQUAL(decisionTwice.offeredParamNamesAtHook("before-surface"), std::set<std::string>{"u"});
    EXPECT_EQUAL(decisionTwice.offeredParamNamesAtHook("after-phys"), std::set<std::string>{"u"});

    // 6) an empty scope is a mistake, not "unscoped"
    Protocol requiresEmpty;
    EXPECT_THROWS(requiresEmpty.requireAt<int>("u", std::set<std::string>{}));
}


CASE("test_negotiator_hook_scoped_config_groups") {

    Negotiator negotiator;

    Protocol offers;
    offers.offerHook("hook-A", "the first hook point");
    offers.offerHook("hook-B", "the second hook point");
    offers.offer<int>("I", "always", "I");
    offers.offerAt<int>("J", "always", "J", {"hook-B"});

    Protocol required;
    required.requireHook("hook-A");
    required.requireHook("hook-B");
    required.require<int>("I");

    std::string groups_str = R"YAML(
    parameters:
      -
        - name: J
          type: INT
          hooks: [hook-A]
      -
        - name: J
          type: INT
          hooks: [hook-B]
    )YAML";
    auto groups = eckit::YAMLConfiguration(groups_str).getSubConfigurations("parameters");

    // the first group asks for J where it is not offered and is rejected, the second is satisfied
    PluginDecision decision = negotiator.negotiate("TestPlugin", offers, required, groups);
    EXPECT_EQUAL(decision.accepted(), true);
    EXPECT_EQUAL(decision.offeredParamNamesAtHook("hook-A"), std::set<std::string>{"I"});
    std::set<std::string> expectedAtB = {"I", "J"};
    EXPECT_EQUAL(decision.offeredParamNamesAtHook("hook-B"), expectedAtB);

    // a malformed scope fails loudly
    std::string bad_groups_str = R"YAML(
    parameters:
      -
        - name: J
          type: INT
          hooks: []
    )YAML";
    auto badGroups = eckit::YAMLConfiguration(bad_groups_str).getSubConfigurations("parameters");
    EXPECT_THROWS(negotiator.negotiate("TestPlugin", offers, required, badGroups));
}


// =================================================================================================
// Per-hook access (writable at some hook points, read-only at others)
// =================================================================================================

namespace {

// the model offers P writable at H1 and read-only at H2, and Q read-only everywhere
Protocol perHookOffers() {
    Protocol offers;
    offers.offerHook("H1");
    offers.offerHook("H2");
    offers.offerAt<int>("P", "always", "P", {"H1"}, /*writable=*/true);
    offers.offerAt<int>("P", "always", "P", {"H2"});
    offers.offer<int>("Q", "always", "Q");
    return offers;
}

// a plugin bound to H1 and H2
Protocol boundToH1H2() {
    Protocol req;
    req.requireHook("H1");
    req.requireHook("H2");
    return req;
}

}  // namespace


CASE("test_negotiator - per-hook access") {

    const Protocol offers = perHookOffers();

    // 1) P written at H1, read at H2: accepted, granted at H1 only
    {
        Negotiator negotiator(WriteBackPolicy::single_writer);
        Protocol req = boundToH1H2();
        req.requireWritableAt<int>("P", {"H1"});
        req.requireAt<int>("P", {"H2"});
        PluginDecision decision = negotiator.negotiate("PluginA", offers, req);
        EXPECT(decision.accepted());
        const auto& p = *decision.offeredParams().begin();
        EXPECT(p.isWritableAt("H1"));
        EXPECT_NOT(p.isWritableAt("H2"));

        auto auth = negotiator.writeAuthorisation();
        EXPECT(auth.isAuthorised("PluginA", "P", "H1"));
        EXPECT_NOT(auth.isAuthorised("PluginA", "P", "H2"));
        EXPECT_EQUAL(auth.writableParamsAt("H1"), std::set<std::string>{"P"});
        EXPECT(auth.writableParamsAt("H2").empty());
    }

    // 2) P written at H2, where the model offers it read-only: rejected
    {
        Negotiator negotiator(WriteBackPolicy::single_writer);
        Protocol req = boundToH1H2();
        req.requireWritableAt<int>("P", {"H2"});
        EXPECT_NOT(negotiator.negotiate("PluginA", offers, req).accepted());
    }

    // 3) P written everywhere (develop's requireWritable): rejected, it is read-only at H2
    {
        Negotiator negotiator(WriteBackPolicy::single_writer);
        Protocol req = boundToH1H2();
        req.requireWritable<int>("P");
        EXPECT_NOT(negotiator.negotiate("PluginA", offers, req).accepted());
    }

    // 4) write-back disabled: rejected even where offered writable
    {
        Negotiator negotiator;
        Protocol req = boundToH1H2();
        req.requireWritableAt<int>("P", {"H1"});
        EXPECT_NOT(negotiator.negotiate("PluginA", offers, req).accepted());
    }

    // 5) Q is not offered writable anywhere: rejected
    {
        Negotiator negotiator(WriteBackPolicy::single_writer);
        Protocol req = boundToH1H2();
        req.requireWritableAt<int>("Q", {"H1"});
        EXPECT_NOT(negotiator.negotiate("PluginA", offers, req).accepted());
    }

    // 6) list form from config: writable at a hook point the plugin is not bound to is rejected
    {
        Negotiator negotiator(WriteBackPolicy::single_writer);
        Protocol req;
        req.requireHook("H2");
        auto groups = eckit::YAMLConfiguration(std::string(R"YAML(
        parameters:
          -
            - name: P
              type: INT
              writable: [H1]
        )YAML")).getSubConfigurations("parameters");
        EXPECT_NOT(negotiator.negotiate("PluginA", offers, req, groups).accepted());
    }

    // 7) list form from config, bound to both: read at H2, written at H1
    {
        Negotiator negotiator(WriteBackPolicy::single_writer);
        Protocol req = boundToH1H2();
        auto groups = eckit::YAMLConfiguration(std::string(R"YAML(
        parameters:
          -
            - name: P
              type: INT
              writable: [H1]
        )YAML")).getSubConfigurations("parameters");
        PluginDecision decision = negotiator.negotiate("PluginA", offers, req, groups);
        EXPECT(decision.accepted());
        EXPECT(negotiator.writeAuthorisation().isAuthorised("PluginA", "P", "H1"));
        EXPECT_NOT(negotiator.writeAuthorisation().isAuthorised("PluginA", "P", "H2"));
    }

    // 8) code read-only at H2 + config writable at H1: merged per hook point
    {
        Negotiator negotiator(WriteBackPolicy::single_writer);
        Protocol req = boundToH1H2();
        req.requireAt<int>("P", {"H2"});
        auto groups = eckit::YAMLConfiguration(std::string(R"YAML(
        parameters:
          -
            - name: P
              type: INT
              hooks: [H1]
              writable: true
        )YAML")).getSubConfigurations("parameters");
        PluginDecision decision = negotiator.negotiate("PluginA", offers, req, groups);
        EXPECT(decision.accepted());
        const auto& p = *decision.offeredParams().begin();
        EXPECT(p.isWritableAt("H1"));
        EXPECT_NOT(p.isWritableAt("H2"));
        EXPECT_EQUAL(decision.hooksOf("P"), (std::set<std::string>{"H1", "H2"}));
    }
}


CASE("test_negotiator - per-hook conflicts") {

    // P offered writable at both hook points
    Protocol offers;
    offers.offerHook("H1");
    offers.offerHook("H2");
    offers.offerAt<int>("P", "always", "P", {"H1", "H2"}, /*writable=*/true);

    auto writerAt = [](const std::string& hook) {
        Protocol req = boundToH1H2();
        req.requireWritableAt<int>("P", {hook});
        return req;
    };
    auto readerAt = [](const std::string& hook) {
        Protocol req = boundToH1H2();
        req.requireAt<int>("P", {hook});
        return req;
    };

    // is the second plugin accepted after the first one, for a policy?
    auto secondAccepted = [&offers](WriteBackPolicy policy, const Protocol& first, const Protocol& second) {
        Negotiator negotiator(policy);
        EXPECT(negotiator.negotiate("First", offers, first).accepted());
        return negotiator.negotiate("Second", offers, second).accepted();
    };

    const std::vector<WriteBackPolicy> policies = {WriteBackPolicy::single_writer,
                                                   WriteBackPolicy::single_writer_strict, WriteBackPolicy::multi_writer,
                                                   WriteBackPolicy::multi_writer_strict};

    for (auto policy : policies) {
        const bool multi  = isMultiWriter(policy);
        const bool strict = isStrictPolicy(policy);

        // different hook points never conflict
        EXPECT(secondAccepted(policy, writerAt("H1"), readerAt("H2")));
        EXPECT(secondAccepted(policy, readerAt("H2"), writerAt("H1")));
        EXPECT(secondAccepted(policy, writerAt("H1"), writerAt("H2")));

        // same hook point: develop's rules
        EXPECT_EQUAL(secondAccepted(policy, writerAt("H1"), writerAt("H1")), multi);
        EXPECT_EQUAL(secondAccepted(policy, writerAt("H1"), readerAt("H1")), !strict);
        EXPECT_EQUAL(secondAccepted(policy, readerAt("H1"), writerAt("H1")), !strict);

        // a plugin never conflicts with itself (written at H1, read at H2)
        Negotiator negotiator(policy);
        Protocol self = boundToH1H2();
        self.requireWritableAt<int>("P", {"H1"});
        self.requireAt<int>("P", {"H2"});
        EXPECT(negotiator.negotiate("Self", offers, self).accepted());
    }
}


//----------------------------------------------------------------------------------------------------------------------

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
