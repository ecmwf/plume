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
#include <iostream>
#include "eckit/config/YAMLConfiguration.h"

#include "eckit/runtime/Main.h"

#include "atlas/field/Field.h"

#include "plume/Manager.h"
#include "plume/data/ModelData.h"
#include "plume/data/ParameterCatalogue.h"


// Prepare a dummy Atlas Field
atlas::Field createAtlasField() {
    std::vector<double> values{1.11, 2.22, 3.33, 4.44};
    atlas::Field field("field", values.data(), atlas::array::make_shape(values.size(), 1));    
    return field;
}


int main(int argc, char** argv) {

    ASSERT_MSG(argc >= 2, "NOTE: this example is for illustration purposes only, error checks are missing!");

    std::cout << "*** Running Example 1 (C++ interface) ***" << std::endl;

    // init eckit main
    eckit::Main::initialise(argc, argv);    
    
    // Plume manager configuration
    char* plumeConfigPath = argv[1];
    eckit::YAMLConfiguration plumeConfig(eckit::PathName{plumeConfigPath});
    plume::Manager::configure(plumeConfig);

    // Negotiate
    plume::Protocol offers;
    offers.offerInt("I", "always", "this is param I");
    offers.offerInt("J", "always", "this is param J");
    offers.offerInt("K", "always", "this is param K");
    offers.offerAtlasField("field_dummy_1", "always", "this is dummy_field");

    offers.offerInt("config-param-1", "always", "this is param config-param-1");
    offers.offerDouble("config-param-2", "always", "this is param config-param-2");
    offers.offerAtlasField("config-param-3", "always", "this is param config-param-3");
    plume::Manager::negotiate(offers);

    // data
    int param_i = 0;
    int param_j = 10;
    int param_k = 100;
    atlas::Field field = createAtlasField();

    int config_param_1 = 9;
    double config_param_2 = 99.99;
    atlas::Field config_param_3 = createAtlasField();

    plume::data::ModelData data;
    data.provideInt("I", &param_i);
    data.provideInt("J", &param_j);
    data.provideInt("K", &param_k);
    data.provideAtlasFieldShared("field_dummy_1", field.get());

    data.provideInt("config-param-1", &config_param_1);
    data.provideDouble("config-param-2", &config_param_2);
    data.provideAtlasFieldShared("config-param-3", config_param_3.get());

    // Feed plugins with the data
    plume::Manager::feedPlugins(data);

    // Run the model for 10 iterations
    for (int i=0; i<10; i++){
        plume::Manager::run();

        // e.g. update parameters..
        param_i++;
        param_j++;
        param_k++;
    }

    // Teardown as necessary
    plume::Manager::teardown();

    // terminate eckit main
    eckit::Main::instance().terminate();

    std::cout << "*** Example 1 complete. ***" << std::endl;
    std::cout << std::endl;
}
