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

#include "plume/data/ModelData.h"


using namespace eckit::testing;

namespace eckit {
namespace test {


CASE("test model data - type checks") {

    plume::data::ModelData data;

    // --------- test type checking ---------
    int paramInt = 1;
    bool paramBool = false;
    float paramFloat = 1.111;
    double paramDouble = 2.222;
    char paramString[] = "some-text";

    // int param
    data.provideInt("param-i", &paramInt);
    EXPECT_NO_THROW(data.getInt("param-i"));
    EXPECT_THROWS(data.getBool("param-i"));
    EXPECT_THROWS(data.getFloat("param-i"));
    EXPECT_THROWS(data.getDouble("param-i"));
    EXPECT_THROWS(data.getString("param-i"));

    // bool param
    data.provideBool("param-b", &paramBool);
    EXPECT_THROWS(data.getInt("param-b"));
    EXPECT_NO_THROW(data.getBool("param-b"));
    EXPECT_THROWS(data.getFloat("param-b"));
    EXPECT_THROWS(data.getDouble("param-b"));
    EXPECT_THROWS(data.getString("param-b"));

    // float param
    data.provideFloat("param-f", &paramFloat);
    EXPECT_THROWS(data.getInt("param-f"));
    EXPECT_THROWS(data.getBool("param-f"));
    EXPECT_NO_THROW(data.getFloat("param-f"));
    EXPECT_THROWS(data.getDouble("param-f"));
    EXPECT_THROWS(data.getString("param-f"));

    // double param
    data.provideDouble("param-d", &paramDouble);
    EXPECT_THROWS(data.getInt("param-d"));
    EXPECT_THROWS(data.getBool("param-d"));
    EXPECT_THROWS(data.getFloat("param-d"));
    EXPECT_NO_THROW(data.getDouble("param-d"));
    EXPECT_THROWS(data.getString("param-d"));

    // string param
    data.provideString("param-s", paramString);
    EXPECT_THROWS(data.getInt("param-s"));
    EXPECT_THROWS(data.getBool("param-s"));
    EXPECT_THROWS(data.getFloat("param-s"));
    EXPECT_THROWS(data.getDouble("param-s"));
    EXPECT_NO_THROW(data.getString("param-s"));

}


CASE("test model data - type checks 2") {

    plume::data::ModelData data;

    EXPECT_THROWS(data.getString("not-existant-key"));
    EXPECT_THROWS(data.getAtlasFieldShared("not-existant-field"));
}

//----------------------------------------------------------------------------------------------------------------------

}  // namespace test
}  // namespace eckit

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}