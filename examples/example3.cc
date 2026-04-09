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


// Prepare a dummy Atlas Field
atlas::Field createAtlasField() {
    std::vector<double> values{1.11, 2.22, 3.33, 4.44};
    atlas::Field field("field", values.data(), atlas::array::make_shape(values.size(), 1));    
    return field;
}


int main(int argc, char** argv) {

    ASSERT_MSG(argc >= 2, "NOTE: this example is for illustration purposes only, error checks are missing!");

    std::cout << "*** Running Example 3 (C++ interface) ***" << std::endl;

    // init eckit main
    eckit::Main::initialise(argc, argv);    
    
    // Plume manager configuration
    char* plumeConfigPath = argv[1];
    eckit::YAMLConfiguration plumeConfig(eckit::PathName{plumeConfigPath});
    plume::Manager::configure(plumeConfig);

    // Negotiate
    plume::Protocol offers;
    offers.offer<int>("I", "always", "this is param I");
    offers.offer<int>("J", "always", "this is param J");
    offers.offer<int>("K", "always", "this is param K");
    offers.offer<atlas::Field>("field_dummy_1", "always", "this is dummy_field");

    offers.offer<int>("config-param-1", "on-request", "this is param config-param-1");
    offers.offer<double>("config-param-2", "on-request", "this is param config-param-2");
    offers.offer<atlas::Field>("config-param-3", "on-request", "this is param config-param-3");

    // Negotiate with plugins
    plume::Manager::negotiate(offers);

    // data
    atlas::Field field = createAtlasField();

    // Declare plume data and add some parameters into it..
    plume::data::ModelData data;
    data.createParam("I", 0);
    data.createParam("J", 10);
    data.createParam("K", 100);
    data.createParam("field_dummy_1", field);


    // parameters requested by the plugins through configuration
    int config_param_1 = 9;
    double config_param_2 = 99.99;
    atlas::Field config_param_3 = createAtlasField();

    data.provideParam("config-param-1", &config_param_1);
    data.provideParam("config-param-2", &config_param_2);
    data.provideParam("config-param-3", &config_param_3);

    // Feed plugins with the data
    plume::Manager::feedPlugins(data);

    // Run the model for 10 iterations
    for (int i=0; i<10; i++){        

        // Update the values
        data.updateParam("I", i);
        data.updateParam("J", 10+i);
        data.updateParam("K", 100+i);

        // run
        plume::Manager::run();
    }

    // Teardown as necessary
    plume::Manager::teardown();

    // terminate eckit main
    eckit::Main::instance().terminate();

    std::cout << "*** Example 3 complete. ***" << std::endl;
    std::cout << std::endl;
}
