/*
 * (C) Copyright 2025- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 *
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */
#include <string>
#include <vector>

#include "atlas/array/ArrayShape.h"
#include "eckit/filesystem/PathName.h"
#include "eckit/mpi/Comm.h"
#include "eckit/testing/Test.h"

#include "nwp_emulator/nwp_data_provider.h"
#include "nwp_emulator/nwp_definitions.h"

using namespace eckit::testing;
using namespace nwp_emulator;

namespace eckit {
namespace test {
CASE("test_reader_setup") {
    GRIBFileReader* dataReader;
    EXPECT_NO_THROW(
        dataReader = new GRIBFileReader(eckit::PathName{std::getenv("TEST_DATA_DIR")}, eckit::mpi::comm().rank(), 0));
    // The parameters should be given to the provider in the order in which they are encoded in the grib source file
    std::vector<std::string> expectedParams{"u,ml,1",   "u,ml,28",  "u,ml,55",   "u,ml,82",
                                            "u,ml,109", "u,ml,136", "100u,sfc,0"};
    // Ensure that the main and secondary readers all share the same setup
    EXPECT_EQUAL(dataReader->getGridName(), "N80");
    EXPECT_EQUAL(dataReader->getParams(), expectedParams);
    delete dataReader;
}
CASE("test_data_reading") {
    NWPDataProvider dataProvider(DataSourceType::GRIB, eckit::PathName{std::getenv("TEST_DATA_DIR")},
                                 eckit::mpi::comm().rank(), 0, eckit::mpi::comm().size());

    std::vector<std::string> expectedFields{"100u", "u"};
    EXPECT_EQUAL(dataProvider.getModelFieldSet().field_names(), expectedFields);

    // With N80 there is 35718 grid points, so 11906 per partition
    EXPECT_EQUAL(dataProvider.getModelFieldSet().field("u").shape(), atlas::array::make_shape(11906, 6));
    EXPECT_EQUAL(dataProvider.getModelFieldSet().field("100u").shape(), atlas::array::make_shape(11906, 1));

    std::vector<FIELD_TYPE_REAL> expectedValuesU{14.5716, 8.74349, 6.9388, -5.17533, 6.08444, 11.8501};
    std::vector<FIELD_TYPE_REAL> expectedValues100U{1.48735, -3.94234, -1.19038, 1.22401, -7.02989, 1.72304};

    int iterCount = 0;  // avoid infinite while loop
    while (dataProvider.getStepData()) {
        iterCount++;
        EXPECT_MSG(iterCount < 3, [=]() {
            std::cerr << "The data provider has iterated through all steps, it should return false now..." << std::endl;
        };);

        auto field100u = atlas::array::make_view<FIELD_TYPE_REAL, 2>(dataProvider.getModelFieldSet().field("100u"));
        auto fieldu    = atlas::array::make_view<FIELD_TYPE_REAL, 2>(dataProvider.getModelFieldSet().field("u"));
        EXPECT(std::abs(fieldu(0, 0) - expectedValuesU[(dataProvider.getStep() - 1) * eckit::mpi::comm().size() +
                                                       eckit::mpi::comm().rank()]) < 1e-4);
        EXPECT(std::abs(field100u(0, 0) - expectedValues100U[(dataProvider.getStep() - 1) * eckit::mpi::comm().size() +
                                                             eckit::mpi::comm().rank()]) < 1e-4);
        eckit::mpi::comm().barrier();
    }
    EXPECT_EQUAL(dataProvider.getStep(), 2);
}
}  // namespace test
}  // namespace eckit

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}