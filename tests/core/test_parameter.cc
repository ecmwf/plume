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
#include <string>
#include <vector>

#include "eckit/testing/Test.h"
#include "eckit/exception/Exceptions.h"
#include "eckit/config/YAMLConfiguration.h"
#include "plume/data/ParameterCatalogue.h"


using namespace eckit::testing;

namespace plume::test {

CASE("test parameter") {

    // valid
    std::string valid_config = R"YAML(
    name: lat
    type: ATLAS_FIELD
    available: on-request
    comment: none
    )YAML";


    // invalid (missing name)
    std::string missing_name = R"YAML(
    type: ATLAS_FIELD
    available: on-request
    comment: none
    )YAML";

    // invalid (missing type)
    std::string missing_type = R"YAML(
    name: lat
    available: on-request
    comment: none
    )YAML";

    std::string missing_avail = R"YAML(
    name: lat
    type: ATLAS_FIELD
    comment: none
    )YAML";

    std::string missing_comment = R"YAML(
    name: lat
    type: ATLAS_FIELD
    available: on-request
    )YAML";

    eckit::YAMLConfiguration config(valid_config);    
    eckit::YAMLConfiguration config_missing_name(missing_name);
    eckit::YAMLConfiguration config_missing_type(missing_type);
    eckit::YAMLConfiguration config_missing_avail(missing_avail);
    eckit::YAMLConfiguration config_missing_comment(missing_comment);


    EXPECT_NO_THROW( plume::data::ParameterDefinition param(config) );

    EXPECT_THROWS( plume::data::ParameterDefinition param(config_missing_name) );
    EXPECT_THROWS( plume::data::ParameterDefinition param(config_missing_type) );
    EXPECT_NO_THROW( plume::data::ParameterDefinition param(config_missing_avail) );
    EXPECT_NO_THROW( plume::data::ParameterDefinition param(config_missing_comment) );
}


CASE("test parameter - hook scope") {

    // absent: unscoped
    eckit::YAMLConfiguration unscoped(std::string(R"YAML(
    name: lat
    type: ATLAS_FIELD
    )YAML"));
    plume::data::ParameterDefinition paramUnscoped(unscoped);
    EXPECT(paramUnscoped.hooks().empty());
    EXPECT_NOT(paramUnscoped.config().has("hooks"));

    // valid scope
    eckit::YAMLConfiguration scoped(std::string(R"YAML(
    name: lat
    type: ATLAS_FIELD
    hooks: [hook-A, hook-B]
    )YAML"));
    plume::data::ParameterDefinition paramScoped(scoped);
    std::set<std::string> expectedAB = {"hook-A", "hook-B"};
    EXPECT_EQUAL(paramScoped.hooks(), expectedAB);

    // invalid scopes: empty list, not a list
    eckit::YAMLConfiguration emptyScope(std::string(R"YAML(
    name: lat
    type: ATLAS_FIELD
    hooks: []
    )YAML"));
    EXPECT_THROWS( plume::data::ParameterDefinition param(emptyScope) );

    eckit::YAMLConfiguration notAList(std::string(R"YAML(
    name: lat
    type: ATLAS_FIELD
    hooks: hook-A
    )YAML"));
    EXPECT_THROWS( plume::data::ParameterDefinition param(notAList) );

    // constructed from arguments: an empty set means unscoped and writes no "hooks" key
    plume::data::ParameterDefinition fromArgs("lat", plume::data::ParameterType::ATLAS_FIELD, "always", "", false,
                                              {"hook-A"});
    EXPECT_EQUAL(fromArgs.hooks(), std::set<std::string>{"hook-A"});
    EXPECT(fromArgs.config().has("hooks"));
    plume::data::ParameterDefinition fromArgsUnscoped("lat", plume::data::ParameterType::ATLAS_FIELD, "always", "");
    EXPECT_NOT(fromArgsUnscoped.config().has("hooks"));

    // merge: scopes unite, an unscoped definition makes the result unscoped
    fromArgs.mergeAccess(
        plume::data::ParameterDefinition("lat", plume::data::ParameterType::ATLAS_FIELD, "", "", false, {"hook-B"}));
    EXPECT_EQUAL(fromArgs.hooks(), expectedAB);
    fromArgs.mergeAccess(plume::data::ParameterDefinition("lat", plume::data::ParameterType::ATLAS_FIELD));
    EXPECT(fromArgs.hooks().empty());
    EXPECT_NOT(fromArgs.config().has("hooks"));

    // the scope is not a strategy option: a derived param keeps matching its strategy
    eckit::YAMLConfiguration derivedScoped(std::string(R"YAML(
    name: u
    type: ATLAS_FIELD
    height: 100
    hooks: [hook-A]
    )YAML"));
    plume::data::ParameterDefinition derived(derivedScoped);
    EXPECT_NOT(derived.strategy().empty());
    EXPECT_EQUAL(derived.hooks(), std::set<std::string>{"hook-A"});
}


CASE("test parameter - per-hook access from config") {

    using plume::data::ParameterDefinition;

    // boolean form: writable at the whole scope
    ParameterDefinition all(eckit::YAMLConfiguration(std::string(R"YAML(
    name: P
    type: ATLAS_FIELD
    hooks: [H1, H2]
    writable: true
    )YAML")));
    EXPECT(all.writable());
    EXPECT(all.writableHooks().empty());
    EXPECT(all.isWritableAt("H1"));
    EXPECT(all.isWritableAt("H2"));

    // list form: writable at H1, read-only at H2
    ParameterDefinition some(eckit::YAMLConfiguration(std::string(R"YAML(
    name: P
    type: ATLAS_FIELD
    hooks: [H1, H2]
    writable: [H1]
    )YAML")));
    EXPECT(some.writable());
    EXPECT_EQUAL(some.writableHooks(), std::set<std::string>{"H1"});
    EXPECT(some.isWritableAt("H1"));
    EXPECT_NOT(some.isWritableAt("H2"));

    // list form without a scope: read-only everywhere but at H1
    ParameterDefinition unscoped(eckit::YAMLConfiguration(std::string(R"YAML(
    name: P
    type: ATLAS_FIELD
    writable: [H1]
    )YAML")));
    EXPECT(unscoped.hooks().empty());
    EXPECT(unscoped.isWritableAt("H1"));
    EXPECT_NOT(unscoped.isWritableAt("H3"));

    // read-only
    ParameterDefinition ro(eckit::YAMLConfiguration(std::string(R"YAML(
    name: P
    type: ATLAS_FIELD
    hooks: [H1]
    )YAML")));
    EXPECT_NOT(ro.writable());
    EXPECT_NOT(ro.isWritableAt("H1"));

    // invalid: empty list, writable hook point outside the scope, not a bool nor a list. The configurations are
    // built outside the EXPECT_* macros: raw string literals inside macro arguments are not portable (their line
    // structure may not survive, which breaks the YAML).
    eckit::YAMLConfiguration emptyList(std::string(R"YAML(
    name: P
    type: ATLAS_FIELD
    writable: []
    )YAML"));
    eckit::YAMLConfiguration outsideScope(std::string(R"YAML(
    name: P
    type: ATLAS_FIELD
    hooks: [H1]
    writable: [H2]
    )YAML"));
    eckit::YAMLConfiguration notAList(std::string(R"YAML(
    name: P
    type: ATLAS_FIELD
    writable: H1
    )YAML"));
    EXPECT_THROWS_AS(ParameterDefinition param(emptyList), eckit::BadValue);
    EXPECT_THROWS_AS(ParameterDefinition param(outsideScope), eckit::BadValue);
    EXPECT_THROWS_AS(ParameterDefinition param(notAList), eckit::BadValue);

    // derived params cannot be writable, in the list form either
    eckit::YAMLConfiguration derivedWritable(std::string(R"YAML(
    name: u
    type: ATLAS_FIELD
    height: 100
    writable: [H1]
    )YAML"));
    EXPECT_THROWS(ParameterDefinition param(derivedWritable));
}


CASE("test parameter - per-hook access merge") {

    using plume::data::ParameterDefinition;
    using plume::data::ParameterType;
    using Hooks = std::set<std::string>;

    auto def = [](bool writable, const Hooks& hooks, const Hooks& writableHooks = {}) {
        return ParameterDefinition("P", ParameterType::ATLAS_FIELD, "", "", writable, hooks, writableHooks);
    };

    // expected per-hook access over the hook points {H1, H2, H3}: "w" writable, "r" read-only
    auto access = [](const ParameterDefinition& p) {
        std::string s;
        for (const auto& h : {"H1", "H2", "H3"}) {
            const bool inScope = p.hooks().empty() || p.hooks().count(h) > 0;
            s += !inScope ? "-" : (p.isWritableAt(h) ? "w" : "r");
        }
        return s;
    };

    struct Case {
        ParameterDefinition lhs, rhs;
        std::string expected;
        bool widened;
    };

    std::vector<Case> cases = {
        // both read-only: scopes unite
        {def(false, {"H1"}), def(false, {"H2"}), "rr-", false},
        // the per-hook access case: writable at H1, read-only at H2 (both orders)
        {def(true, {"H1"}), def(false, {"H2"}), "wr-", false},
        {def(false, {"H2"}), def(true, {"H1"}), "wr-", false},
        // scoped writes are pinned to their scope: widening the scope does not widen the writes
        {def(true, {"H1"}), def(false, {"H1", "H2"}), "wr-", false},
        // both writable: writes unite
        {def(true, {"H1"}), def(true, {"H2"}), "ww-", false},
        // unscoped read + scoped write: read everywhere, write at H1
        {def(false, {}), def(true, {"H1"}), "wrr", false},
        // unscoped read + unscoped write: plain "writable wins", no warning
        {def(false, {}), def(true, {}), "www", false},
        // unscoped write overrides a scoped read-only definition: warning
        {def(true, {}), def(false, {"H2"}), "www", true},
        {def(false, {"H2"}), def(true, {}), "www", true},
        // list form merged with read-only
        {def(true, {"H1", "H2"}, {"H1"}), def(false, {"H3"}), "wrr", false},
    };

    for (auto& c : cases) {
        const bool widened = c.lhs.mergeAccess(c.rhs);
        EXPECT_EQUAL(access(c.lhs), c.expected);
        EXPECT_EQUAL(widened, c.widened);
    }

    // normalisation: writable at the whole scope is written as "writable: true"
    auto p = def(true, {"H1"});
    p.mergeAccess(def(true, {"H2"}));
    EXPECT(p.writableHooks().empty());
    EXPECT(p.config().isBoolean("writable"));
    EXPECT(p.config().getBool("writable"));

    // ... and writable at some hook points only as a list
    auto q = def(true, {"H1"});
    q.mergeAccess(def(false, {"H2"}));
    EXPECT(q.config().isStringList("writable"));
    EXPECT(q.config().getStringVector("writable") == std::vector<std::string>{"H1"});

    // the merged config can be parsed back to the same access
    ParameterDefinition reparsed(q.config());
    EXPECT_EQUAL(access(reparsed), "wr-");
}

//----------------------------------------------------------------------------------------------------------------------

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}