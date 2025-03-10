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
#include "atlas/util/function/VortexRollup.h"
#include "eckit/filesystem/PathName.h"
#include "eckit/mpi/Comm.h"
#include "eckit/testing/Test.h"

#include "nwp_emulator/nwp_data_provider.h"
#include "nwp_emulator/nwp_definitions.h"

using namespace eckit::testing;
using namespace nwp_emulator;

namespace eckit {
namespace test {
CASE("test_focus_area") {
    atlas::PointLonLat p1{10.12, 65.59}, p2{187.10, 50.73};

    // area crossing the longitude 0/360
    FocusArea areaA{326.70, 47.91, 48.35, 72.98};
    EXPECT(areaA.contains(p1));
    EXPECT_NOT(areaA.contains(p2));

    // area not crossing the longitude 0/360
    FocusArea areaB{155.0, 225.0, 34.5, 71.5};
    EXPECT(areaB.contains(p2));
    EXPECT_NOT(areaB.contains(p1));
}
CASE("test_reader_setup") {
    std::string configPath(std::getenv("TEST_DATA_DIR"));
    ConfigReader* dataReader;
    EXPECT_NO_THROW(
        dataReader = new ConfigReader(eckit::PathName{configPath + "valid_config.yml"}, eckit::mpi::comm().rank(), 0););
    std::vector<std::string> expectedParams{"100u,sfc,0", "u,ml,1", "u,ml,2", "u,ml,3", "u,ml,4", "u,ml,5",
                                            "v,ml,1",     "v,ml,2", "v,ml,3", "v,ml,4", "v,ml,5"};
    EXPECT_EQUAL(dataReader->getGridName(), "N80");
    EXPECT_EQUAL(dataReader->getParams(), expectedParams);
    delete dataReader;
}
CASE("test_invalid_config") {
    std::string configPath(std::getenv("TEST_DATA_DIR"));
    EXPECT_THROWS(
        ConfigReader dataReader(eckit::PathName{configPath + "invalid_config.yml"}, eckit::mpi::comm().rank(), 0));
}
CASE("test_data_generation") {
    std::string configPath(std::getenv("TEST_DATA_DIR"));
    NWPDataProvider dataProvider(DataSourceType::CONFIG, eckit::PathName{configPath + "valid_config.yml"},
                                 eckit::mpi::comm().rank(), 0, eckit::mpi::comm().size());

    std::vector<std::string> expectedFields{"100u", "u", "v"};
    EXPECT_EQUAL(dataProvider.getModelFieldSet().field_names(), expectedFields);

    // With N80 there is 35718 grid points, so 11906 per partition
    EXPECT_EQUAL(dataProvider.getModelFieldSet().field("u").shape(), atlas::array::make_shape(11906, 5));
    EXPECT_EQUAL(dataProvider.getModelFieldSet().field("v").shape(), atlas::array::make_shape(11906, 5));
    EXPECT_EQUAL(dataProvider.getModelFieldSet().field("100u").shape(), atlas::array::make_shape(11906, 1));

    auto lonlat =
        atlas::array::make_view<double, 2>(dataProvider.getModelFieldSet().field("u").functionspace().lonlat());
    FocusArea europe{155.0, 225.0, 34.5, 71.5};

    int iterCount = 0;  // avoid infinite while loop
    while (dataProvider.getStepData()) {
        iterCount++;
        EXPECT_MSG(iterCount < 3, [=]() {
            std::cerr << "The data provider has iterated through all steps, it should return false now..." << std::endl;
        };);

        // 1. Vortex rollup & area mask
        auto field100u = atlas::array::make_view<FIELD_TYPE_REAL, 2>(dataProvider.getModelFieldSet().field("100u"));
        // Point outside of the focus area should remain zeros
        EXPECT_NOT(europe.contains(atlas::PointLonLat{lonlat(0, 0), lonlat(0, 1)}));
        EXPECT(std::abs(field100u(0, 0)) < 1e-4);

        if (eckit::mpi::comm().rank() == 0) {
            // Europe is entirely owned by partition 0
            EXPECT(europe.contains(atlas::PointLonLat{lonlat(1132, 0), lonlat(1132, 1)}));
            EXPECT(std::abs(field100u(1132, 0) - atlas::util::function::vortex_rollup(lonlat(1132, 0), lonlat(1132, 1),
                                                                                      (iterCount - 1) * 1.1)) < 1e-4);
        }
        auto fieldu = atlas::array::make_view<FIELD_TYPE_REAL, 2>(dataProvider.getModelFieldSet().field("u"));
        // 2. Random
        EXPECT(fieldu(0, 1) - 1 > 1e-6);
        EXPECT(2 - fieldu(0, 1) > 1e-6);
        // 3. Step
        if (eckit::mpi::comm().rank() == 0) {
            EXPECT(std::abs(fieldu(1132, 1) - 10 - (iterCount - 1)) < 1e-6);
        }
        // 4. Sinc
        EXPECT(fieldu(0, 0) + 1 > 1e-6);
        EXPECT(10 - fieldu(0, 0) > 1e-6);
        // 5. Gaussian
        EXPECT(fieldu(0, 4) > 1);
        EXPECT(fieldu(0, 4) < 2 + 1e-6);

        eckit::mpi::comm().barrier();
    }
    EXPECT_EQUAL(dataProvider.getStep(), 2);
}
}  // namespace test
}  // namespace eckit

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}