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
atlas::Field createAtlasField(const std::string& name) {
    std::vector<double> values{1.11, 2.22, 3.33, 4.44};
    atlas::Field field(name, values.data(), atlas::array::make_shape(values.size(), 1));    
    return field;
}


int main(int argc, char** argv) {

    ASSERT_MSG(argc >= 2, "NOTE: this example is for illustration purposes only, error checks are missing!");

    std::cout << "*** Running Example 2 (C++ interface) ***" << std::endl;

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
    offers.offerAtlasField("field_dummy_1", "on-request", "this is dummy_field");
    plume::Manager::negotiate(offers);

    // data
    int param_i = 0;
    int param_j = 10;
    int param_k = 100;
    plume::data::ModelData data;

    // Insert some int parameters (regardless of whether 
    // they are going to be requested by plugins or not..)
    data.provideInt("I", &param_i);
    data.provideInt("J", &param_j);
    data.provideInt("K", &param_k);

    // NOTE: After the negotiation, the manager knows which parameters 
    // have been requested by all activated plugins (i.e. plugins that 
    // negotiated with success)
    // 
    // => This information can be used to insert "on-request" params 
    //    (i.e parameters that are inserted only if requested by plugins)
    //
    // For example, if "field_dummy_1" has been requested, then insert it:
    std::string name = "field_dummy_1";
    atlas::Field field;
    if ( plume::Manager::isParamRequested(name) ) {
        field = createAtlasField(name);
        data.provideAtlasFieldShared(name, field.get());
    }

    // Once data is ready.. feed the plugins!
    plume::Manager::feedPlugins(data);

    // Run the activated plugins for 10 iterations
    for (int i=0; i<10; i++){
        // std::cout << "From manager: " << data.atlasFieldShared("field_dummy_1") << std::endl;
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

    std::cout << "*** Example 2 complete. ***" << std::endl;
    std::cout << std::endl;
}
