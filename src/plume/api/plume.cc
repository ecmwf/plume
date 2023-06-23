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
#include <functional>
#include <iostream>
#include <map>
#include <vector>

#include "eckit/config/YAMLConfiguration.h"
#include "eckit/config/LocalConfiguration.h"
#include "eckit/exception/Exceptions.h"
#include "eckit/filesystem/PathName.h"
#include "eckit/linalg/Tensor.h"
#include "eckit/runtime/Main.h"

#include "atlas/array.h"
#include "atlas/field/Field.h"
#include "atlas/field/detail/FieldImpl.h"

#include "plume.h"
#include "plume/Manager.h"
#include "plume/data/ParameterCatalogue.h"
#include "plume/data/ModelData.h"

/* Error handling */

static std::string g_current_error_str;
static plume_failure_handler_t g_failure_handler = nullptr;
static void* g_failure_handler_context           = nullptr;
static bool plume_initialised                    = false;


/** Returns the error string */
const char* plume_error_string(int err) {
    switch (err) {
        case PLUME_SUCCESS:
            return "Success";
        case PLUME_ERROR_GENERAL_EXCEPTION:
        case PLUME_ERROR_UNKNOWN_EXCEPTION:
            return g_current_error_str.c_str();
        default:
            return "<unknown>";
    };
}

int innerWrapFn(std::function<int()> f) {
    return f();
}

int innerWrapFn(std::function<void()> f) {
    f();
    return PLUME_SUCCESS;
}

/** Wraps API functions and properly set errors to be reported
 * to the C interface */
template <typename FN>
int wrapApiFunction(FN f) {

    try {
        return innerWrapFn(f);
    }
    catch (eckit::Exception& e) {
        eckit::Log::error() << "Caught exception on C-C++ API boundary: " << e.what() << std::endl;
        g_current_error_str = e.what();
        if (g_failure_handler) {
            g_failure_handler(g_failure_handler_context, PLUME_ERROR_GENERAL_EXCEPTION);
        }
        return PLUME_ERROR_GENERAL_EXCEPTION;
    }
    catch (std::exception& e) {
        eckit::Log::error() << "Caught exception on C-C++ API boundary: " << e.what() << std::endl;
        g_current_error_str = e.what();
        if (g_failure_handler) {
            g_failure_handler(g_failure_handler_context, PLUME_ERROR_GENERAL_EXCEPTION);
        }
        return PLUME_ERROR_GENERAL_EXCEPTION;
    }
    catch (...) {
        eckit::Log::error() << "Caught unknown on C-C++ API boundary" << std::endl;
        g_current_error_str = "Unrecognised and unknown exception";
        if (g_failure_handler) {
            g_failure_handler(g_failure_handler_context, PLUME_ERROR_UNKNOWN_EXCEPTION);
        }
        return PLUME_ERROR_UNKNOWN_EXCEPTION;
    }

    ASSERT(false);
}

int plume_set_failure_handler(plume_failure_handler_t handler, void* context) {
    return wrapApiFunction([handler, context] {
        g_failure_handler         = handler;
        g_failure_handler_context = context;
        eckit::Log::info() << "plume-plugins setting failure handler fn." << std::endl;
    });
}

#ifdef __cplusplus
extern "C" {
#endif


// Interface Protocol handle
struct plume_protocol_handle_t {
    plume_protocol_handle_t(plume::Protocol* protocol) : impl_(protocol) {}
    ~plume_protocol_handle_t() noexcept(false) {}
    std::unique_ptr<plume::Protocol> impl_;
};

// Model handle
struct plume_manager_handle_t {
    plume_manager_handle_t(plume::Manager* manager) : impl_(manager) {}
    ~plume_manager_handle_t() noexcept(false) {}
    std::unique_ptr<plume::Manager> impl_;
};

// PLUME data
struct plume_data_handle_t {
    plume_data_handle_t(plume::data::ModelData* run_data) : impl_{run_data} {}
    ~plume_data_handle_t() noexcept(false) {}
    plume::data::ModelData* impl_;

    // May / may not own underlying data
    bool ownsData_;
};


// initialization
int plume_initialise(int argc, char** argv) {
    return wrapApiFunction([argc, argv] {
        eckit::Log::info() << "*** Initialising Plume C-API ***" << std::endl;
        eckit::Main::initialise(argc, argv);

        if (plume_initialised) {
            throw eckit::UnexpectedState("Initialising Plume library twice!", Here());
        }

        if (!plume_initialised) {
            eckit::Main::initialise(1, const_cast<char**>(argv));
            plume_initialised = true;
        }
    });
}

int plume_finalise() {
    return wrapApiFunction([] {
        eckit::Log::info() << "*** Finalising Plume C-API ***" << std::endl;
        if (!plume_initialised) {
            throw eckit::UnexpectedState("Plume library not initialised!", Here());
        }
        else {
            plume_initialised = false;
        }
    });
}
// --------------------------------------------------------------------------------------


// ------------------------------------ PLUME protocol -----------------------------------
int plume_protocol_create_handle(plume_protocol_handle_t** h) {
    return wrapApiFunction([h] {
        *h = new plume_protocol_handle_t(new plume::Protocol);
        ASSERT(*h);
        ASSERT((*h)->impl_);
    });
}

int plume_protocol_offer_int(plume_protocol_handle_t* h, const char* name, const char* avail, const char* comment) {
    return wrapApiFunction([h, name, avail, comment] {
        ASSERT(h);
        ASSERT((h)->impl_);
        h->impl_->offerInt(name, avail, comment);
    });
}
int plume_protocol_offer_bool(plume_protocol_handle_t* h, const char* name, const char* avail, const char* comment) {
    return wrapApiFunction([h, name, avail, comment] {
        ASSERT(h);
        ASSERT((h)->impl_);
        h->impl_->offerBool(name, avail, comment);
    });
}
int plume_protocol_offer_float(plume_protocol_handle_t* h, const char* name, const char* avail, const char* comment) {
    return wrapApiFunction([h, name, avail, comment] {
        ASSERT(h);
        ASSERT((h)->impl_);
        h->impl_->offerFloat(name, avail, comment);
    });
}
int plume_protocol_offer_double(plume_protocol_handle_t* h, const char* name, const char* avail, const char* comment) {
    return wrapApiFunction([h, name, avail, comment] {
        ASSERT(h);
        ASSERT((h)->impl_);
        h->impl_->offerDouble(name, avail, comment);
    });
}
int plume_protocol_offer_atlas_field(plume_protocol_handle_t* h, const char* name, const char* avail, const char* comment) {
    return wrapApiFunction([h, name, avail, comment] {
        ASSERT(h);
        ASSERT((h)->impl_);
        h->impl_->offerAtlasField(name, avail, comment);
    });
}

int plume_protocol_delete_handle(plume_protocol_handle_t* h) {
    return wrapApiFunction([&h] {
        if (h) {
            delete h;
            h = nullptr;
        }
    });
}
// --------------------------------------------------------------------------------------



// ------------------------------------ PLUME manager ----------------------------------------

// Create a plume-plugin manager handle
int plume_manager_create_handle(plume_manager_handle_t** h) {
    return wrapApiFunction([h] {
        *h = new plume_manager_handle_t(new plume::Manager);
        ASSERT(*h);
        ASSERT((*h)->impl_);
    });
}

int plume_manager_configure(plume_manager_handle_t* h, const char* config_path) {

        return wrapApiFunction([h, config_path] {
        ASSERT(h);
        ASSERT((h)->impl_);

        // Manager Configuration
        eckit::YAMLConfiguration mgrConfig{eckit::PathName(config_path)};
        h->impl_->configure(mgrConfig);

    });
}

int plume_manager_negotiate(plume_manager_handle_t* h, plume_protocol_handle_t* p) {
    return wrapApiFunction([h, p] {
        ASSERT(h);
        ASSERT((h)->impl_);

        ASSERT(p);
        ASSERT((p)->impl_);

        h->impl_->negotiate( *(p->impl_) );

    });
}

// feed plugins with data
int plume_manager_feed_plugins(plume_manager_handle_t* h, plume_data_handle_t* fdata) {
    return wrapApiFunction([h, fdata] {
        ASSERT(h);
        ASSERT((h)->impl_);

        h->impl_->feedPlugins(*(fdata->impl_));
    });
}

int plume_manager_active_fields(plume_manager_handle_t* h, char** str_in) {

    return wrapApiFunction([h, &str_in] {
        ASSERT(h);
        ASSERT((h)->impl_);

        // Decide how we want to wrap the data..
        auto req_params = h->impl_->getActiveParams();

        // concatenate param names into a CS-string
        std::string tmp;
        for (const auto& p : req_params) {
            tmp = tmp + "," + p;
        }

        // allocate and return
        *str_in = strcpy(new char[tmp.length() + 1], tmp.c_str());
    });
}

int plume_manager_active_data_catalogue(plume_manager_handle_t* h, void** active_data_catalogue) {
    return wrapApiFunction([h, &active_data_catalogue] {
        ASSERT(h);
        ASSERT((h)->impl_);
 
        *active_data_catalogue = new eckit::LocalConfiguration(h->impl_->getActiveDataCatalogue().getConfig());
    });
}

int plume_manager_is_param_requested(plume_manager_handle_t* h, const char* name, bool* requested) {
    return wrapApiFunction([h, name, &requested] {
        ASSERT(h);
        ASSERT((h)->impl_);
 
        std::string namestr{name};
        *requested = h->impl_->isParamRequested(namestr);
        std::cout << "-->>> param: " << name << " requested? : " << *requested << std::endl;
    });
}

int plume_manager_run(plume_manager_handle_t* h) {
    return wrapApiFunction([h] {
        ASSERT(h);
        ASSERT((h)->impl_);

        h->impl_->run();
    });
}

int plume_manager_teardown(plume_manager_handle_t* h) {
    return wrapApiFunction([h] {
        ASSERT(h);
        ASSERT((h)->impl_);

        h->impl_->teardown();
    });    
}

int plume_manager_delete_handle(plume_manager_handle_t* h) {
    return wrapApiFunction([&h] {
        if (h) {
            delete h;
            h = nullptr;
        }
    });
}
// --------------------------------------------------------------------------------------

// ------------------------------------ PLUME data ----------------------------------------
int plume_data_create_handle_t(plume_data_handle_t** h) {
    return wrapApiFunction([h] {
        *h = new plume_data_handle_t(new plume::data::ModelData);
        ASSERT(*h);
        ASSERT((*h)->impl_);
        (*h)->ownsData_ = true;
    });
}

int plume_data_create_handle_from_ptr(plume_data_handle_t** h, void* cptr) {
    return wrapApiFunction([h, cptr] {
        *h = new plume_data_handle_t(static_cast<plume::data::ModelData*>(cptr));
        ASSERT(*h);
        ASSERT((*h)->impl_);
        (*h)->ownsData_ = false;
    });
}

int plume_data_delete_handle(plume_data_handle_t* h) {
    return wrapApiFunction([&h] {
        if (h) {

            // delete data if owned
            if (h->ownsData_) {
                delete h->impl_;
                h->impl_ = nullptr;
            }

            // delete handle
            delete h;
            h = nullptr;
        }
    });
}



// ------------------ Data creators ------------------
// insert int param into the data structure
int plume_data_create_int(plume_data_handle_t* h, const char* name, int param) {
    return wrapApiFunction([h, name, param] {
        h->impl_->createInt(name, param);
    });
}

// insert int param into the data structure
int plume_data_create_bool(plume_data_handle_t* h, const char* name, bool param) {
    return wrapApiFunction([h, name, param] { 
        h->impl_->createBool(name, param); 
    });
}

// insert double param into the data structure
int plume_data_create_float(plume_data_handle_t* h, const char* name, float param) {
    return wrapApiFunction([h, name, param] { 
        h->impl_->createFloat(name, param); 
    });
}

// insert double param into the data structure
int plume_data_create_double(plume_data_handle_t* h, const char* name, double param) {
    return wrapApiFunction([h, name, param] { 
        h->impl_->createDouble(name, param); 
    });
}


// ------------------ Data updaters ------------------
// insert int param into the data structure
int plume_data_update_int(plume_data_handle_t* h, const char* name, int param) {
    return wrapApiFunction([h, name, param] {
        h->impl_->updateInt(name, param);
    });
}

// insert int param into the data structure
int plume_data_update_bool(plume_data_handle_t* h, const char* name, bool param) {
    return wrapApiFunction([h, name, param] { 
        h->impl_->updateBool(name, param); 
    });
}

// insert double param into the data structure
int plume_data_update_float(plume_data_handle_t* h, const char* name, float param) {
    return wrapApiFunction([h, name, param] { 
        h->impl_->updateFloat(name, param); 
    });
}

// insert double param into the data structure
int plume_data_update_double(plume_data_handle_t* h, const char* name, double param) {
    return wrapApiFunction([h, name, param] { 
        h->impl_->updateDouble(name, param); 
    });
}



// ------------------ Data providers ------------------
// insert int param into the data structure
int plume_data_provide_int(plume_data_handle_t* h, const char* name, int* param) {
    return wrapApiFunction([h, name, param] {
        h->impl_->provideInt(name, param);
    });
}

// insert int param into the data structure
int plume_data_provide_bool(plume_data_handle_t* h, const char* name, bool* param) {
    return wrapApiFunction([h, name, param] { h->impl_->provideBool(name, param); });
}

// insert double param into the data structure
int plume_data_provide_float(plume_data_handle_t* h, const char* name, float* param) {
    return wrapApiFunction([h, name, param] { h->impl_->provideFloat(name, param); });
}

// insert double param into the data structure
int plume_data_provide_double(plume_data_handle_t* h, const char* name, double* param) {
    return wrapApiFunction([h, name, param] { h->impl_->provideDouble(name, param); });
}

// -------- Atlas objects
int plume_data_provide_atlas_field_shared(plume_data_handle_t* h, const char* name, void* ptr) {
    return wrapApiFunction([h, name, ptr] {
        auto field_ptr = static_cast<atlas::Field::Implementation*>(ptr);
        h->impl_->provideAtlasFieldShared(name, field_ptr);
    });
}


// ----------------- Data view "updaters" (Plugin API) -----------------
int plume_data_get_shared_atlas_field(plume_data_handle_t* h, const char* name, void** ptr) {
    return wrapApiFunction([h, name, ptr] { *ptr = h->impl_->getAtlasFieldShared(name).get(); });
}

int plume_data_get_int(plume_data_handle_t* h, const char* name, int* val) {
    return wrapApiFunction([h, name, val] { *val = h->impl_->getInt(name); });
}

int plume_data_get_bool(plume_data_handle_t* h, const char* name, bool* val) {
    return wrapApiFunction([h, name, val] { *val = h->impl_->getBool(name); });
}

int plume_data_get_float(plume_data_handle_t* h, const char* name, float* val) {
    return wrapApiFunction([h, name, val] { *val = h->impl_->getFloat(name); });
}

int plume_data_get_double(plume_data_handle_t* h, const char* name, double* val) {
    return wrapApiFunction([h, name, val] { *val = h->impl_->getDouble(name); });
}

int plume_data_print(plume_data_handle_t* h) {
    return wrapApiFunction([h] { h->impl_->print(); });
}

// --------------------------------------------------------------------------------------


#ifdef __cplusplus
}
#endif
