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
#include "eckit/testing/Test.h"
#include "eckit/config/YAMLConfiguration.h"
#include "plume/data/Parameter.h"


using namespace eckit::testing;

namespace eckit {
namespace test {

CASE("test parameter") {

    // valid
    std::string valid_config = R"YAML({"name": "lat", "type": "ATLAS_FIELD", "available": "on-request", "comment": "none"})YAML";

    // invalid
    std::string missing_name = R"YAML({"type": "ATLAS_FIELD", "available": "on-request", "comment": "none"})YAML";
    std::string missing_type = R"YAML({"name": "lat", "available": "on-request", "comment": "none"})YAML";
    std::string missing_avail = R"YAML({"name": "lat", "type": "ATLAS_FIELD", "comment": "none"})YAML";
    std::string missing_comment = R"YAML({"name": "lat", "type": "ATLAS_FIELD", "available": "on-request"})YAML";


    eckit::YAMLConfiguration config(valid_config);    
    eckit::YAMLConfiguration config_missing_name(missing_name);
    eckit::YAMLConfiguration config_missing_type(missing_type);
    eckit::YAMLConfiguration config_missing_avail(missing_avail);
    eckit::YAMLConfiguration config_missing_comment(missing_comment);


    EXPECT_NO_THROW( plume::data::Parameter param(config) );

    EXPECT_THROWS( plume::data::Parameter param(config_missing_name) );
    EXPECT_THROWS( plume::data::Parameter param(config_missing_type) );
    EXPECT_NO_THROW( plume::data::Parameter param(config_missing_avail) );
    EXPECT_NO_THROW( plume::data::Parameter param(config_missing_comment) );
}

//----------------------------------------------------------------------------------------------------------------------

}  // namespace test
}  // namespace eckit

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}