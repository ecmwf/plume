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
#include "eckit/runtime/Main.h"
#include "plume/api/plume.h"
#include "test_api_utils.h"


using namespace eckit::testing;

namespace plume::test {

CASE("test_lib_api") {

    // this should not throw and return success
    EXPECT_PLUME_CODE_SUCCESS( plume_initialise(eckit::Main::instance().argc(), eckit::Main::instance().argv())); // Init plume

    // a second call to initialise should still NOT throw an exception, but return an error code through the C interface
    EXPECT_PLUME_CODE_FAILURE( plume_initialise(eckit::Main::instance().argc(), eckit::Main::instance().argv())); // Init plume

    // finalise plume
    EXPECT_PLUME_CODE_SUCCESS( plume_finalise());

    // finalise plume again, this should not throw an exception, but return an error code through the C interface
    EXPECT_PLUME_CODE_FAILURE( plume_finalise());
}

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}