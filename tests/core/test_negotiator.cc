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
#include "eckit/config/YAMLConfiguration.h"
#include "eckit/testing/Test.h"

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


//----------------------------------------------------------------------------------------------------------------------

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
