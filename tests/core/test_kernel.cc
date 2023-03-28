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
#include "eckit/config/LocalConfiguration.h"
#include "plume/Kernel.h"


using namespace eckit::testing;

namespace eckit {
namespace test {


// A dummy Kernel
class DummyKernel : public plume::Kernel {
public:
    DummyKernel(const eckit::Configuration& config): plume::Kernel(config) {};
    virtual ~DummyKernel() {};
    constexpr static const char* type() { return "dummy_kernel"; }
    virtual void setup() override {};
    virtual void teardown() override {};
    virtual void run() override {};
};


CASE("test plugin 1") {

    DummyKernel kernel{eckit::LocalConfiguration()};

    EXPECT_NO_THROW( kernel.setup() );
    EXPECT_NO_THROW( kernel.run() );
    EXPECT_NO_THROW( kernel.teardown() );

}

//----------------------------------------------------------------------------------------------------------------------

}  // namespace test
}  // namespace eckit

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}