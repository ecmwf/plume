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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "plume/api/plume.h"


int main(int argc, char** argv){

    printf("NOTE: this example is for illustration purposes only, error checks are missing!\n");
    assert(argc >= 2);

    char* plugin_config  = argv[1];
    
    plume_protocol_handle_t* proto_handle;
    plume_manager_handle_t* mgr_handle;
    plume_data_handle_t* data_handle;

    // example of parameters to pass..
    int param_i = 0;
    int param_j = 10;
    int param_k = 100;

    printf("*** Running Example 1 (C interface) ***\n");

    // Init plume
    plume_initialise(argc, argv);

    // offer parameters    
    plume_protocol_create_handle(&proto_handle);
    plume_protocol_offer_int(proto_handle, "I", "always", "this is param I");
    plume_protocol_offer_int(proto_handle, "J", "always", "this is param J");
    plume_protocol_offer_int(proto_handle, "K", "always", "this is param K");

    // configure and Negotiate
    plume_manager_create_handle(&mgr_handle);
    plume_manager_configure(mgr_handle, plugin_config);
    plume_manager_negotiate(mgr_handle, proto_handle);

    // Provide data as needed
    plume_data_create_handle_t(&data_handle);
    plume_data_provide_int(data_handle, "I", &param_i);
    plume_data_provide_int(data_handle, "J", &param_j);
    plume_data_provide_int(data_handle, "K", &param_k);
    plume_data_print(data_handle);

    // Feed the plugins (i.e. each plugin grabs its own share of data)
    plume_manager_feed_plugins(mgr_handle, data_handle);

    // Run all plugins for several iterations
    for(int i=0; i<10; i++) {
      plume_manager_run(mgr_handle);

      // e.g. update parameters..
      param_i++;
      param_j++;
      param_k++;
    }

    // Delete handles
    plume_protocol_delete_handle(proto_handle);
    plume_manager_delete_handle(mgr_handle);
    plume_data_delete_handle(data_handle);

    // Finalise plume
    plume_finalise();

    printf("*** Example 1 complete. ***\n\n");
}
