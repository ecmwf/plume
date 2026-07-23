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
#include <optional>
#include <string>
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
#include "plume/PlumeTag.h"
#include "plume/PlumeState.h"
#include "plume/data/ParameterCatalogue.h"
#include "plume/data/ModelData.h"

/* Error handling */

static std::string g_current_error_str;
static plume_failure_handler_t g_failure_handler = nullptr;
static void* g_failure_handler_context           = nullptr;
static bool plume_initialised                    = false;

namespace {

plume::PlumeTag plumeTagFromId(plume_tag_id_t tag_id) {
    switch (tag_id) {
#define PLUME_TAG(tag_id, tag_name) case PLUME_TAG_ID_##tag_id: return plume::PlumeTag::tag_id;
#include "plume/TagDefinitions.def"
#undef PLUME_TAG
        default:
            throw eckit::BadParameter("Invalid plume tag id: " + std::to_string(static_cast<int>(tag_id)), Here());
    }
}

}  // namespace


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

const char* plume_tag_name(plume_tag_id_t tag_id) {
    switch (tag_id) {
#define PLUME_TAG(tag_id, tag_name) case PLUME_TAG_ID_##tag_id: return tag_name;
#include "plume/TagDefinitions.def"
#undef PLUME_TAG
        default:
            return nullptr;
    }
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


// ------------------------------------ PLUME state --------------------------------------
int plume_state_current_name(char** state_name) {
    return wrapApiFunction([&state_name] {
        std::string stateName = plume::PlumeState::instance().currentName();
        *state_name = strcpy(new char[stateName.length() + 1], stateName.c_str());
    });
}

int plume_state_current_parent(char** state_parent) {
    return wrapApiFunction([&state_parent] {
        std::string stateParent = plume::PlumeState::instance().currentParent();
        *state_parent = strcpy(new char[stateParent.length() + 1], stateParent.c_str());
    });
}

int plume_state_current_iteration(int64_t* iteration) {
    return wrapApiFunction([&iteration] {
        *iteration = static_cast<int64_t>(plume::PlumeState::instance().currentIteration());
    });
}

int plume_state_current_iteration_rel(int64_t* iteration_rel) {
    return wrapApiFunction([&iteration_rel] {
        *iteration_rel = static_cast<int64_t>(plume::PlumeState::instance().currentRelativeIteration());
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
        h->impl_->offer<int>(name, avail, comment);
    });
}
int plume_protocol_offer_bool(plume_protocol_handle_t* h, const char* name, const char* avail, const char* comment) {
    return wrapApiFunction([h, name, avail, comment] {
        ASSERT(h);
        ASSERT((h)->impl_);
        h->impl_->offer<bool>(name, avail, comment);
    });
}
int plume_protocol_offer_float(plume_protocol_handle_t* h, const char* name, const char* avail, const char* comment) {
    return wrapApiFunction([h, name, avail, comment] {
        ASSERT(h);
        ASSERT((h)->impl_);
        h->impl_->offer<float>(name, avail, comment);
    });
}
int plume_protocol_offer_double(plume_protocol_handle_t* h, const char* name, const char* avail, const char* comment) {
    return wrapApiFunction([h, name, avail, comment] {
        ASSERT(h);
        ASSERT((h)->impl_);
        h->impl_->offer<double>(name, avail, comment);
    });
}
int plume_protocol_offer_atlas_field(plume_protocol_handle_t* h, const char* name, const char* avail, const char* comment) {
    return wrapApiFunction([h, name, avail, comment] {
        ASSERT(h);
        ASSERT((h)->impl_);
        h->impl_->offer<atlas::Field>(name, avail, comment);
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

int plume_manager_configure_from_string(plume_manager_handle_t* h, const char* config_string) {
    return wrapApiFunction([h, config_string] {
        ASSERT(h);
        ASSERT((h)->impl_);

        // Manager Configuration
        eckit::YAMLConfiguration mgrConfig{std::string(config_string)};
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
    });
}

int plume_manager_is_plugin_activated(plume_manager_handle_t* h, const char* name, bool* activated) {
    return wrapApiFunction([h, name, &activated] {
        ASSERT(h);
        ASSERT((h)->impl_);

        std::string namestr{name};
        *activated = h->impl_->isPluginActivated(namestr);
    });
}

int plume_manager_current_state_name(plume_manager_handle_t* h, char** state_name) {
    return wrapApiFunction([h, &state_name] {
        ASSERT(h);
        ASSERT((h)->impl_);

        std::string stateName = h->impl_->currentStateName();
        *state_name = strcpy(new char[stateName.length() + 1], stateName.c_str());
    });
}

int plume_manager_current_state_parent(plume_manager_handle_t* h, char** state_parent) {
    return wrapApiFunction([h, &state_parent] {
        ASSERT(h);
        ASSERT((h)->impl_);

        std::string stateParent = h->impl_->currentStateParent();
        *state_parent = strcpy(new char[stateParent.length() + 1], stateParent.c_str());
    });
}

int plume_manager_current_state_iteration(plume_manager_handle_t* h, int64_t* iteration) {
    return wrapApiFunction([h, &iteration] {
        ASSERT(h);
        ASSERT((h)->impl_);

        *iteration = static_cast<int64_t>(h->impl_->currentStateIteration());
    });
}

int plume_manager_current_state_iteration_rel(plume_manager_handle_t* h, int64_t* iteration_rel) {
    return wrapApiFunction([h, &iteration_rel] {
        ASSERT(h);
        ASSERT((h)->impl_);

        *iteration_rel = static_cast<int64_t>(h->impl_->currentStateIterationRel());
    });
}

int plume_manager_run(plume_manager_handle_t* h, plume_tag_id_t tag_id, bool has_parent, plume_tag_id_t parent_id) {
    return wrapApiFunction([h, tag_id, has_parent, parent_id] {
        ASSERT(h);
        ASSERT((h)->impl_);

        const plume::PlumeTag tag_enum = plumeTagFromId(tag_id);
        const std::optional<plume::PlumeTag> parent_enum = has_parent ? std::optional<plume::PlumeTag>(plumeTagFromId(parent_id))
                                                                       : std::nullopt;
        h->impl_->run(tag_enum, parent_enum);
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
        h->impl_->createParam(name, param);
    });
}

// insert int param into the data structure
int plume_data_create_bool(plume_data_handle_t* h, const char* name, bool param) {
    return wrapApiFunction([h, name, param] { 
        h->impl_->createParam(name, param); 
    });
}

// insert double param into the data structure
int plume_data_create_float(plume_data_handle_t* h, const char* name, float param) {
    return wrapApiFunction([h, name, param] { 
        h->impl_->createParam(name, param); 
    });
}

// insert double param into the data structure
int plume_data_create_double(plume_data_handle_t* h, const char* name, double param) {
    return wrapApiFunction([h, name, param] { 
        h->impl_->createParam(name, param); 
    });
}


// ------------------ Data updaters ------------------
// insert int param into the data structure
int plume_data_update_int(plume_data_handle_t* h, const char* name, int param) {
    return wrapApiFunction([h, name, param] {
        h->impl_->updateParam(name, param);
    });
}

// insert int param into the data structure
int plume_data_update_bool(plume_data_handle_t* h, const char* name, bool param) {
    return wrapApiFunction([h, name, param] { 
        h->impl_->updateParam(name, param); 
    });
}

// insert double param into the data structure
int plume_data_update_float(plume_data_handle_t* h, const char* name, float param) {
    return wrapApiFunction([h, name, param] { 
        h->impl_->updateParam(name, param); 
    });
}

// insert double param into the data structure
int plume_data_update_double(plume_data_handle_t* h, const char* name, double param) {
    return wrapApiFunction([h, name, param] { 
        h->impl_->updateParam(name, param); 
    });
}



// ------------------ Data providers ------------------
// insert int param into the data structure
int plume_data_provide_int(plume_data_handle_t* h, const char* name, int* param) {
    return wrapApiFunction([h, name, param] {
        h->impl_->provideParam(name, param);
    });
}

// insert int param into the data structure
int plume_data_provide_bool(plume_data_handle_t* h, const char* name, bool* param) {
    return wrapApiFunction([h, name, param] { h->impl_->provideParam(name, param); });
}

// insert double param into the data structure
int plume_data_provide_float(plume_data_handle_t* h, const char* name, float* param) {
    return wrapApiFunction([h, name, param] { h->impl_->provideParam(name, param); });
}

// insert double param into the data structure
int plume_data_provide_double(plume_data_handle_t* h, const char* name, double* param) {
    return wrapApiFunction([h, name, param] { h->impl_->provideParam(name, param); });
}

// -------- Atlas objects
int plume_data_provide_atlas_field_shared(plume_data_handle_t* h, const char* name, void* ptr) {
    return wrapApiFunction([h, name, ptr] {
        auto field_ptr = static_cast<atlas::Field::Implementation*>(ptr);
        h->impl_->provideParam(name, field_ptr);
    });
}


// ----------------- Data view "updaters" (Plugin API) -----------------
int plume_data_get_shared_atlas_field(plume_data_handle_t* h, const char* name, void** ptr) {
    return wrapApiFunction([h, name, ptr] { *ptr = h->impl_->getParam<atlas::Field>(name).get(); });
}

int plume_data_get_int(plume_data_handle_t* h, const char* name, int* val) {
    return wrapApiFunction([h, name, val] { *val = h->impl_->getParam<int>(name); });
}

int plume_data_get_bool(plume_data_handle_t* h, const char* name, bool* val) {
    return wrapApiFunction([h, name, val] { *val = h->impl_->getParam<bool>(name); });
}

int plume_data_get_float(plume_data_handle_t* h, const char* name, float* val) {
    return wrapApiFunction([h, name, val] { *val = h->impl_->getParam<float>(name); });
}

int plume_data_get_double(plume_data_handle_t* h, const char* name, double* val) {
    return wrapApiFunction([h, name, val] { *val = h->impl_->getParam<double>(name); });
}

int plume_data_print(plume_data_handle_t* h) {
    return wrapApiFunction([h] { h->impl_->print(); });
}

int plume_data_set_updated(plume_data_handle_t* h, const int count, const char** names) {
    std::vector<std::string> params;
    for (int i = 0; i < count; ++i) {
        params.push_back(names[i]);
    }
    return wrapApiFunction([h, params] {h->impl_->setUpdated(params); });
}

// delete a C-string allocated by Plume API functions
int plume_delete_string(char* str) {
    return wrapApiFunction([str] {
        if (str) {
            delete[] str;
        }
    });
}

// --------------------------------------------------------------------------------------


#ifdef __cplusplus
}
#endif
