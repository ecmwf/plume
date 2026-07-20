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
#include <set>
#include "eckit/config/YAMLConfiguration.h"
#include "eckit/testing/Test.h"

#include "ManagerTestAccess.h"
#include "plume/Manager.h"


using namespace eckit::testing;

namespace plume::test {

// === Data offer builder ==========================================================================
// Fluent builder for the "offered:" YAML block used in negotiate() calls.
// Method chaining lets each variant be expressed as a one-liner starting from
// the shared IJK base, with no repeated YAML boilerplate.

struct OfferBuilder {
    std::string yaml_{"offered:\n"};

    OfferBuilder& add(const std::string& name, const std::string& type, bool writable = false) {
        yaml_ += "  - name: " + name + "\n";
        yaml_ += "    type: " + type + "\n";
        yaml_ += "    available: always\n";
        yaml_ += "    comment: none\n";
        if (writable)
            yaml_ += "    writable: true\n";
        return *this;
    }

    // Implicit conversion so an OfferBuilder can be passed wherever a std::string is expected.
    operator std::string() const { return yaml_; }
};

// Base factory: the three INT params hard-required by SimplePlugin::negotiate().
static OfferBuilder offersIJK() {
    return OfferBuilder{}.add("I", "INT").add("J", "INT").add("K", "INT");
}

// === Shared data offer configurations ============================================================

// I, J, K only
static const std::string kOffersIJK = offersIJK();
// I, J, K + u  (z absent → u;hl;100 derivation cannot be satisfied)
static const std::string kOffersIJKU = offersIJK().add("u", "ATLAS_FIELD");
// I, J, K + u + z  (z is the geopotential dependency for u;hl;100 derivation)
static const std::string kOffersIJKUZ = offersIJK().add("u", "ATLAS_FIELD").add("z", "ATLAS_FIELD");
// I, J, K + W_INT (offered as writable)
static const std::string kOffersIJKWritableInt = offersIJK().add("W_INT", "INT", /*writable=*/true);
// I, J, K + W_RO (offered read-only)
static const std::string kOffersIJKReadonlyInt = offersIJK().add("W_RO", "INT");
// I, J, K + u + z + W_INT (writable) — multi-plugin write-back conflict tests
static const std::string kOffersIJKUZWritableInt =
    offersIJK().add("u", "ATLAS_FIELD").add("z", "ATLAS_FIELD").add("W_INT", "INT", true);
// I, J, K + u (writable) + z — write-derived-read interaction tests
static const std::string kOffersIJKWritableAtlas =
    offersIJK().add("u", "ATLAS_FIELD", /*writable=*/true).add("z", "ATLAS_FIELD");

// === Shared manager config builders ==============================================================

// Two plugins both requesting W_INT as writable; only the policy varies.
static std::string mgrBothWriteWInt(const std::string& policy) {
    return "write-back-policy: " + policy + R"YAML(
plugins:
- lib: simple_plugins
  name: SimplePlugin
  parameters:
    -
      - name: W_INT
        type: INT
        writable: true
  core-config: {}
- lib: simple_plugins
  name: SimpleDerivedPlugin
  parameters:
    -
      - name: W_INT
        type: INT
        writable: true
  core-config: {}
)YAML";
}

// Writer (SimplePlugin, W_INT writable) + reader (SimpleDerivedPlugin, W_INT read-only).
static std::string mgrWriteReadWInt(const std::string& policy) {
    return "write-back-policy: " + policy + R"YAML(
plugins:
- lib: simple_plugins
  name: SimplePlugin
  parameters:
    -
      - name: W_INT
        type: INT
        writable: true
  core-config: {}
- lib: simple_plugins
  name: SimpleDerivedPlugin
  parameters:
    -
      - name: W_INT
        type: INT
  core-config: {}
)YAML";
}

// Writer (SimplePlugin, u:ATLAS_FIELD writable) + derived-reader (SimpleDerivedPlugin, u;hl;100).
static std::string mgrWriteUDerived(const std::string& policy) {
    return "write-back-policy: " + policy + R"YAML(
plugins:
- lib: simple_plugins
  name: SimplePlugin
  parameters:
    -
      - name: u
        type: ATLAS_FIELD
        writable: true
  core-config: {}
- lib: simple_plugins
  name: SimpleDerivedPlugin
  core-config: {}
)YAML";
}

// === Test helper =================================================================================

// Reset Manager state, assert unconfigured, configure, then negotiate.
// Asserts that both configure and negotiate do not throw.
static void resetConfigureNegotiate(const std::string& mgrConf, const std::string& dataConf) {
    ManagerTestAccess::reset();
    EXPECT_NOT(plume::Manager::isConfigured());
    EXPECT_NO_THROW(plume::Manager::configure(eckit::YAMLConfiguration(mgrConf)));
    EXPECT_NO_THROW(plume::Manager::negotiate(eckit::YAMLConfiguration(dataConf)));
}

//--------------------------------------------------------------------------------------------------

CASE("test_invalid_plugin_configuration_parameters") {

    // invalid YAML
    std::string mgr_conf_str = R"YAML(
    plugins:
     - lib: simple_plugins
       name: SimplePlugin
       parameters: 999
       core-config: -
        -
    )YAML";

    EXPECT_THROWS(eckit::YAMLConfiguration mgr_cfg_invalid_json(mgr_conf_str));

    // wrong "parameters" type
    std::string mgr_conf_str2 = R"YAML(
    plugins:
    - lib: simple_plugins
      name: SimplePlugin
      parameters: 999
      core-config: {}
    )YAML";

    eckit::YAMLConfiguration mgr_cfg_invalid_param_type(mgr_conf_str2);

    // configure
    EXPECT_EQUAL(plume::Manager::isConfigured(), false);
    EXPECT_THROWS(plume::Manager::configure(mgr_cfg_invalid_param_type));
    EXPECT_EQUAL(plume::Manager::isConfigured(), false);
}


CASE("test_valid_plugin_configuration_parameters") {

    std::string mgr_conf_str = R"YAML(
    plugins:
    - lib: simple_plugins
      name: SimplePlugin
      parameters:
      - 
        - name: I
          type: INT
        - name: J
          type: INT
      -
        - name: JJJ
          type: INT
        - name: J
          type: INT
        - name: KKMM
          type: INT
      -
        - name: XYZ
          type: INT
        - name: K
          type: INT
      core-config: {}
    )YAML";

    // unique data config — this test exercises groups with non-standard params
    std::string data_conf_str = R"YAML(
    offered:
      - name: I
        type: INT
        available: always
        comment: none-1
      - name: J
        type: INT
        available: always
        comment: none-1
      - name: JJJ
        type: INT
        available: always
        comment: none-2
      - name: XYZ
        type: INT
        available: always
        comment: none-2
      - name: K
        type: INT
        available: always
        comment: none-3
    )YAML";

    // configure
    EXPECT_EQUAL(plume::Manager::isConfigured(), false);
    plume::Manager::configure(eckit::YAMLConfiguration(mgr_conf_str));
    EXPECT_EQUAL(plume::Manager::isConfigured(), true);

    plume::Manager::negotiate(eckit::YAMLConfiguration(data_conf_str));
    EXPECT_EQUAL(plume::Manager::isConfigured(), true);

    // expected active params: I, J, XYZ, K
    EXPECT_EQUAL(plume::Manager::getActiveParams().size(), 4);
}


CASE("test_derived_parameter") {

    std::string mgr_conf_str = R"YAML(
    plugins:
    - lib: simple_plugins
      name: SimplePlugin
      parameters:
        - 
          - name: u
            type: ATLAS_FIELD
            height: 100
    )YAML";

    resetConfigureNegotiate(mgr_conf_str, kOffersIJKUZ);

    std::set<std::string> expected = {"I", "J", "K", "u", "u;hl;100", "z"};
    std::set<std::string> activeParams(plume::Manager::getActiveParams().begin(),
                                       plume::Manager::getActiveParams().end());
    EXPECT_EQUAL(activeParams, expected);
}


CASE("test_invalid_derived_parameter") {

    std::string mgr_conf_str = R"YAML(
    plugins:
    - lib: simple_plugins
      name: SimplePlugin
      parameters:
        - 
          - name: u
            type: ATLAS_FIELD
            invalid_option: 100
    )YAML";

    ManagerTestAccess::reset();
    EXPECT_NOT(plume::Manager::isConfigured());
    EXPECT_NO_THROW(plume::Manager::configure(eckit::YAMLConfiguration(mgr_conf_str)));
    EXPECT_THROWS(plume::Manager::negotiate(eckit::YAMLConfiguration(kOffersIJKU)));
}


CASE("test_cannot_derive_parameter") {

    // z is absent from the offers so u;hl;100 cannot be derived — plugin not activated.
    std::string mgr_conf_str = R"YAML(
    plugins:
    - lib: simple_plugins
      name: SimplePlugin
      parameters:
        - 
          - name: u
            type: ATLAS_FIELD
            height: 100
    )YAML";

    resetConfigureNegotiate(mgr_conf_str, kOffersIJKU);

    std::set<std::string> expected = {};
    std::set<std::string> activeParams(plume::Manager::getActiveParams().begin(),
                                       plume::Manager::getActiveParams().end());
    EXPECT_EQUAL(activeParams, expected);
}


CASE("test_writable_param_satisfied_via_config") {

    // Model offers W_INT as writable; plugin config requires it as writable.
    // write-back-policy: single-writer enables write-back.
    // Expected: group accepted, W_INT in active params.

    std::string mgr_conf_str = R"YAML(
    write-back-policy: single-writer
    plugins:
    - lib: simple_plugins
      name: SimplePlugin
      parameters:
        -
          - name: W_INT
            type: INT
            writable: true
      core-config: {}
    )YAML";

    resetConfigureNegotiate(mgr_conf_str, kOffersIJKWritableInt);
    EXPECT_EQUAL(plume::Manager::getActiveParams().count("W_INT"), 1);
}


CASE("test_writable_param_rejected_via_config") {

    // Model offers W_RO as read-only; plugin config requires it as writable.
    // Rejection is because the offer is read-only, not the policy.
    // Expected: group rejected, W_RO not in active params.

    std::string mgr_conf_str = R"YAML(
    write-back-policy: single-writer
    plugins:
    - lib: simple_plugins
      name: SimplePlugin
      parameters:
        -
          - name: W_RO
            type: INT
            writable: true
      core-config: {}
    )YAML";

    resetConfigureNegotiate(mgr_conf_str, kOffersIJKReadonlyInt);
    EXPECT_EQUAL(plume::Manager::getActiveParams().count("W_RO"), 0);
}


CASE("test_writable_offered_readonly_required_via_config") {

    // Model offers W_INT as writable; plugin config requires it with no writable key (read-only).
    // No write-back-policy needed: the plugin is not requesting write access.
    // Expected: group accepted — a writable offer does not force plugins to request write access.

    std::string mgr_conf_str = R"YAML(
    plugins:
    - lib: simple_plugins
      name: SimplePlugin
      parameters:
        -
          - name: W_INT
            type: INT
      core-config: {}
    )YAML";

    resetConfigureNegotiate(mgr_conf_str, kOffersIJKWritableInt);
    EXPECT_EQUAL(plume::Manager::getActiveParams().count("W_INT"), 1);
}


CASE("test_write_back_policy_disabled_default") {

    // No write-back-policy key in manager config -> disabled by default.
    // Plugin requests W_INT as writable; expected: group rejected, plugin not activated.

    std::string mgr_conf_str = R"YAML(
    plugins:
    - lib: simple_plugins
      name: SimplePlugin
      parameters:
        -
          - name: W_INT
            type: INT
            writable: true
      core-config: {}
    )YAML";

    resetConfigureNegotiate(mgr_conf_str, kOffersIJKWritableInt);
    EXPECT_EQUAL(plume::Manager::getActivePluginNames().size(), 0u);
    EXPECT_EQUAL(plume::Manager::getActiveParams().count("W_INT"), 0);
}


CASE("test_write_back_policy_single_writer") {

    // write-back-policy: single-writer — two plugins both request W_INT as writable.
    // Expected: first plugin (SimplePlugin) accepted, second (SimpleDerivedPlugin) rejected.

    resetConfigureNegotiate(mgrBothWriteWInt("single-writer"), kOffersIJKUZWritableInt);

    auto activePluginNames = plume::Manager::getActivePluginNames();
    EXPECT_EQUAL(activePluginNames.size(), 1u);
    EXPECT_EQUAL(activePluginNames[0], std::string("SimplePlugin"));
    EXPECT_EQUAL(plume::Manager::getActiveParams().count("W_INT"), 1);
}


CASE("test_write_back_policy_invalid_string") {

    // An unrecognised policy string must be caught at configure() time.
    std::string mgr_conf_str = R"YAML(
    write-back-policy: unknown-policy
    plugins: []
    )YAML";

    ManagerTestAccess::reset();
    EXPECT_THROWS(plume::Manager::configure(eckit::YAMLConfiguration(mgr_conf_str)));
}


CASE("test_writable_conflict_multiple_plugins_both_accepted") {

    // write-back-policy: multi-writer — both plugins request W_INT as writable.
    // Expected: both accepted; run order matches config order.

    resetConfigureNegotiate(mgrBothWriteWInt("multi-writer"), kOffersIJKUZWritableInt);

    auto activePluginNames = plume::Manager::getActivePluginNames();
    EXPECT_EQUAL(activePluginNames.size(), 2u);
    EXPECT_EQUAL(activePluginNames[0], std::string("SimplePlugin"));
    EXPECT_EQUAL(activePluginNames[1], std::string("SimpleDerivedPlugin"));
}


CASE("test_write_read_same_param_non_strict_allowed") {

    // Writer (SimplePlugin, W_INT writable) + reader (SimpleDerivedPlugin, W_INT read-only).
    // Policy: single-writer (non-strict) — both accepted, run order logged as warning.

    resetConfigureNegotiate(mgrWriteReadWInt("single-writer"), kOffersIJKUZWritableInt);

    auto activePluginNames = plume::Manager::getActivePluginNames();
    EXPECT_EQUAL(activePluginNames.size(), 2u);
    EXPECT_EQUAL(activePluginNames[0], std::string("SimplePlugin"));
    EXPECT_EQUAL(activePluginNames[1], std::string("SimpleDerivedPlugin"));
}


CASE("test_write_read_same_param_strict_reader_rejected") {

    // Writer (SimplePlugin, W_INT writable) + reader (SimpleDerivedPlugin, W_INT read-only).
    // Policy: single-writer-strict — reader is rejected because a writer holds the param.

    resetConfigureNegotiate(mgrWriteReadWInt("single-writer-strict"), kOffersIJKUZWritableInt);

    auto activePluginNames = plume::Manager::getActivePluginNames();
    EXPECT_EQUAL(activePluginNames.size(), 1u);
    EXPECT_EQUAL(activePluginNames[0], std::string("SimplePlugin"));
}


CASE("test_write_derived_read_non_strict_allowed") {

    // Writer (SimplePlugin, u:ATLAS_FIELD writable) +
    // Derived-reader (SimpleDerivedPlugin, u;hl;100 derived from u).
    // Policy: single-writer (non-strict) — both accepted, run order logged.

    resetConfigureNegotiate(mgrWriteUDerived("single-writer"), kOffersIJKWritableAtlas);

    auto activePluginNames = plume::Manager::getActivePluginNames();
    EXPECT_EQUAL(activePluginNames.size(), 2u);
    EXPECT_EQUAL(activePluginNames[0], std::string("SimplePlugin"));
    EXPECT_EQUAL(activePluginNames[1], std::string("SimpleDerivedPlugin"));
}


CASE("test_write_derived_read_strict_derived_reader_rejected") {

    // Writer (SimplePlugin, u:ATLAS_FIELD writable) +
    // Derived-reader (SimpleDerivedPlugin, u;hl;100 derived from u).
    // Policy: single-writer-strict — derived reader rejected because a writer holds the source.

    resetConfigureNegotiate(mgrWriteUDerived("single-writer-strict"), kOffersIJKWritableAtlas);

    auto activePluginNames = plume::Manager::getActivePluginNames();
    EXPECT_EQUAL(activePluginNames.size(), 1u);
    EXPECT_EQUAL(activePluginNames[0], std::string("SimplePlugin"));
}


//--------------------------------------------------------------------------------------------------

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
