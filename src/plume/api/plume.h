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
#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* Plume C-interface */

/* --- Plume tags (shared C/C++/Fortran definitions) --- */
typedef enum plume_tag_id_t {
#define PLUME_TAG(tag_id, tag_name) PLUME_TAG_ID_##tag_id,
#include "plume/TagDefinitions.def"
#undef PLUME_TAG
    PLUME_TAG_ID_COUNT
} plume_tag_id_t;

/**
 * @brief Get canonical tag name for tag ID
 *
 * @param tag_id Tag identifier
 * @return Canonical tag name (e.g. "run_lvl1") or NULL for invalid IDs
 */
const char* plume_tag_name(plume_tag_id_t tag_id);


/* Return Codes */
enum PlumeErrorValues
{
    PLUME_SUCCESS                 = 0,
    PLUME_ERROR_GENERAL_EXCEPTION = 1,
    PLUME_ERROR_UNKNOWN_EXCEPTION = 2
};

/**
 * @brief  Returns a human-readable error message for the
 * last error given an error code
 *
 * \param err Error code
 * \returns Error message
 */
const char* plume_error_string(int err);

/** @brief Error handler callback function signature
 *
 * \param context Error handler context
 * \param error_code Error code
 */
typedef void (*plume_failure_handler_t)(void* context, int error_code);

/**
 * @brief Sets an error handler which will be called on error
 * with the supplied context and an error code
 *
 * \param handler Error handler function
 * \param context Error handler context
 *
 * To be called like so:
 *
 *     void handle_failure(void* context, int error_code) {
 *        fprintf(stderr, "Error: %s\n", plume_error_string(error_code));
 *        clean_up();
 *        exit(1);
 *    }
 *    plume_set_failure_handler(handle_failure, NULL);
 */
int plume_set_failure_handler(plume_failure_handler_t handler, void* context);
// --------------------------------------------------------------------------------------


/* --- Plume handles --- */
struct plume_protocol_handle_t;
typedef struct plume_protocol_handle_t plume_protocol_handle_t;

struct plume_manager_handle_t;
typedef struct plume_manager_handle_t plume_manager_handle_t;

struct plume_data_handle_t;
typedef struct plume_data_handle_t plume_data_handle_t;


/* --- Plume Library --- */

/**
 * @brief Initialise
 *
 * @param argc argc
 * @param argv argv
 * @return Error code
 */
int plume_initialise(int argc, char** argv);

/**
 * @brief Finalise the handle
 *
 * @return Error code
 */
int plume_finalise();


/* --- Plume State (singleton) --- */

/**
 * @brief Get current state name from the global Plume state singleton
 *
 * @param state_name Current state name (allocated C-string)
 * @return Error code
 */
int plume_state_current_name(char** state_name);

/**
 * @brief Get current parent state name from the global Plume state singleton
 *
 * @param state_parent Current parent state name (allocated C-string)
 * @return Error code
 */
int plume_state_current_parent(char** state_parent);

/**
 * @brief Get current absolute iteration from the global Plume state singleton
 *
 * @param iteration Current absolute iteration
 * @return Error code
 */
int plume_state_current_iteration(int64_t* iteration);

/**
 * @brief Get current relative iteration from the global Plume state singleton
 *
 * @param iteration_rel Current relative iteration
 * @return Error code
 */
int plume_state_current_iteration_rel(int64_t* iteration_rel);


/* --- Plume Protocol --- */
int plume_protocol_create_handle(plume_protocol_handle_t** h);
int plume_protocol_offer_int(plume_protocol_handle_t* h, const char* name, const char* avail, const char* comment);
int plume_protocol_offer_bool(plume_protocol_handle_t* h, const char* name, const char* avail, const char* comment);
int plume_protocol_offer_float(plume_protocol_handle_t* h, const char* name, const char* avail, const char* comment);
int plume_protocol_offer_double(plume_protocol_handle_t* h, const char* name, const char* avail, const char* comment);
int plume_protocol_offer_atlas_field(plume_protocol_handle_t* h, const char* name, const char* avail, const char* comment);
int plume_protocol_delete_handle(plume_protocol_handle_t* h);

/* --- Plume Manager --- */

/**
 * @brief Create a plume manager handle
 *
 * @param h Handle
 * @return Error code
 */
int plume_manager_create_handle(plume_manager_handle_t** h);

/**
 * @brief create data handle from existing data
 *
 */

/**
 * @brief create data handle from existing data
 *
 * @param h Handle
 * @param cptr ptr to existing data
 * @return Error code
 */
int plume_data_create_handle_from_ptr(plume_data_handle_t** h, void* cptr);

/**
 * @brief Configure the manager with a configuration file
 *
 * @param h Handle
 * @param config_path Path to manager configuration file
 * @return Error code
 */
int plume_manager_configure(plume_manager_handle_t* h, const char* config_path);

/**
 * @brief Configure the manager with a configuration string
 *
 * @param h Handle
 * @param config_string manager configuration string
 * @return Error code
 */
int plume_manager_configure_from_string(plume_manager_handle_t* h, const char* config_string);

/**
 * @brief Load all the plugins
 *
 * @param h Handle
 * @param p Protocol handle
 * @return Error code
 */
int plume_manager_negotiate(plume_manager_handle_t* h, plume_protocol_handle_t* p);

/**
 * @brief Feed data to active plugins
 *
 * @param h Handle
 * @param fdata Handle of data
 * @return Error code
 */
int plume_manager_feed_plugins(plume_manager_handle_t* h, plume_data_handle_t* fdata);

/**
 * @brief Fields requested by all activate plugins
 *
 * @param h Handle
 * @param str_in CSV string of active fields
 * @return Error code
 */
int plume_manager_active_fields(plume_manager_handle_t* h, char** str_in);

/**
 * @brief Fields requested by all activate plugins (as catalogue)
 *
 * @param h Handle
 * @param data_catalogue_ptr Catalogue of data
 * @param active_data_catalogue Catalogue of active data
 * @return Error code
 */
int plume_manager_active_data_catalogue(plume_manager_handle_t* h, void** active_data_catalogue);

/**
 * @brief Checks if a Specific parameter has been requested
 * 
 * @param h 
 * @param name 
 * @param requested 
 * @return int 
 */
int plume_manager_is_param_requested(plume_manager_handle_t* h, const char* name, bool* requested);

/** * @brief Check if a plugin is activated
 *
 * @param h Handle
 * @param name Name of plugin
 * @param activated Pointer to boolean indicating if plugin is activated
 * @return Error code
 */
int plume_manager_is_plugin_activated(plume_manager_handle_t* h, const char* name, bool* activated);

/**
 * @brief Get current manager state name
 *
 * @param h Handle
 * @param state_name Current state name (allocated C-string)
 * @return Error code
 */
int plume_manager_current_state_name(plume_manager_handle_t* h, char** state_name);

/**
 * @brief Get current manager parent state name
 *
 * @param h Handle
 * @param state_parent Current parent state name (allocated C-string)
 * @return Error code
 */
int plume_manager_current_state_parent(plume_manager_handle_t* h, char** state_parent);

/**
 * @brief Get current manager state absolute iteration
 *
 * @param h Handle
 * @param iteration Current state absolute iteration
 * @return Error code
 */
int plume_manager_current_state_iteration(plume_manager_handle_t* h, int64_t* iteration);

/**
 * @brief Get current manager state iteration relative to parent
 *
 * @param h Handle
 * @param iteration_rel Current state relative iteration
 * @return Error code
 */
int plume_manager_current_state_iteration_rel(plume_manager_handle_t* h, int64_t* iteration_rel);

/**
 * @brief Run all plugins using typed tag IDs
 *
 * @param h Handle
 * @param tag_id Tag ID for this run step
 * @param has_parent Whether parent_id should be used
 * @param parent_id Parent tag ID for nested runs (ignored when has_parent is false)
 * @return Error code
 */
int plume_manager_run(plume_manager_handle_t* h, plume_tag_id_t tag_id, bool has_parent, plume_tag_id_t parent_id);

/**
 * @brief Teardown plugins
 * 
 * @param h Handle
 * @return int 
 */
int plume_manager_teardown(plume_manager_handle_t* h);

/**
 * @brief Destroy the plume manager handle
 *
 * @param h Handle
 * @return Error code
 */
int plume_manager_delete_handle(plume_manager_handle_t* h);


/* --- Plume Data --- */

/**
 * @brief Create a plume data handle
 *
 * @param h Handle
 * @return Error code
 */
int plume_data_create_handle_t(plume_data_handle_t** h);

/**
 * @brief Delete a plume data handle
 *
 * @param h Handle
 * @return Error code
 */
int plume_data_delete_handle(plume_data_handle_t* h);



/* ----------------- Data creators ----------------- */
/**
 * @brief Create parameters (int)
 *
 * @param h Handle
 * @param name Name of parameter
 * @param param Parameter
 * @return Error code
 */
int plume_data_create_int(plume_data_handle_t* h, const char* name, int param);

/**
 * @brief Create parameters (bool)
 *
 * @param h Handle
 * @param name Name of parameter
 * @param param parameter
 * @return Error code
 */
int plume_data_create_bool(plume_data_handle_t* h, const char* name, bool param);

/**
 * @brief Create parameters (float)
 *
 * @param h Handle
 * @param name Name of parameter
 * @param param parameter
 * @return Error code
 */
int plume_data_create_float(plume_data_handle_t* h, const char* name, float param);

/**
 * @brief Create parameters (double)
 *
 * @param h Handle
 * @param name Name of parameter
 * @param param parameter
 * @return Error code
 */
int plume_data_create_double(plume_data_handle_t* h, const char* name, double param);


/* ----------------- Data updaters ----------------- */
/**
 * @brief Update parameters (int)
 *
 * @param h Handle
 * @param name Name of parameter
 * @param param Parameter
 * @return Error code
 */
int plume_data_update_int(plume_data_handle_t* h, const char* name, int param);

/**
 * @brief Update parameters (bool)
 *
 * @param h Handle
 * @param name Name of parameter
 * @param param parameter
 * @return Error code
 */
int plume_data_update_bool(plume_data_handle_t* h, const char* name, bool param);

/**
 * @brief Update parameters (float)
 *
 * @param h Handle
 * @param name Name of parameter
 * @param param parameter
 * @return Error code
 */
int plume_data_update_float(plume_data_handle_t* h, const char* name, float param);

/**
 * @brief Update parameters (double)
 *
 * @param h Handle
 * @param name Name of parameter
 * @param param parameter
 * @return Error code
 */
int plume_data_update_double(plume_data_handle_t* h, const char* name, double param);




/* ----------------- Data providers ----------------- */
/**
 * @brief Insert parameters (int)
 *
 * @param h Handle
 * @param name Name of parameter
 * @param param parameter
 * @return Error code
 */
int plume_data_provide_int(plume_data_handle_t* h, const char* name, int* param);

/**
 * @brief Insert parameters (bool)
 *
 * @param h Handle
 * @param name Name of parameter
 * @param param parameter
 * @return Error code
 */
int plume_data_provide_bool(plume_data_handle_t* h, const char* name, bool* param);

/**
 * @brief Insert parameters (float)
 *
 * @param h Handle
 * @param name Name of parameter
 * @param param parameter
 * @return Error code
 */
int plume_data_provide_float(plume_data_handle_t* h, const char* name, float* param);

/**
 * @brief Insert parameters (double)
 *
 * @param h Handle
 * @param name Name of parameter
 * @param param parameter
 * @return Error code
 */
int plume_data_provide_double(plume_data_handle_t* h, const char* name, double* param);

/**
 * @brief Insert Atlas Field
 *
 * @param h Handle
 * @param name Name of field
 * @param ptr Pointer to Field
 * @return Error code
 */
int plume_data_provide_atlas_field_shared(plume_data_handle_t* h, const char* name, void* ptr);

/**
 * @brief C pointer of a selected field
 *
 * @param h
 * @param name
 * @param ptr
 * @return int
 */
int plume_data_get_shared_atlas_field(plume_data_handle_t* h, const char* name, void** ptr);


/* ----------------- Data view "updaters" (Plugin API) ----------------- */
/**
 * @brief Get (int)
 *
 * @param h
 * @param name
 * @param val
 * @return int
 */
int plume_data_get_int(plume_data_handle_t* h, const char* name, int* val);

/**
 * @brief Get (bool)
 *
 * @param h
 * @param name
 * @param val
 * @return int
 */
int plume_data_get_bool(plume_data_handle_t* h, const char* name, bool* val);

/**
 * @brief Get (float)
 *
 * @param h
 * @param name
 * @param val
 * @return int
 */
int plume_data_get_float(plume_data_handle_t* h, const char* name, float* val);

/**
 * @brief Get (double)
 *
 * @param h
 * @param name
 * @param val
 * @return int
 */
int plume_data_get_double(plume_data_handle_t* h, const char* name, double* val);


/* ----------------- Utils ----------------- */
/**
 * @brief Print plume data
 *
 * @param h Handle
 * @return Error code
 */
int plume_data_print(plume_data_handle_t* h);

/**
 * @brief Set params updated flag
 *
 * @param h Handle
 * @param count Number of parameters updated
 * @param names The names of the updated parameters
 * @return Error code
 */
int plume_data_set_updated(plume_data_handle_t* h, const int count, const char** names);

/**
 * @brief Delete a C-string allocated by Plume API functions
 *
 * @param str Pointer to C-string allocated by functions like plume_manager_current_state_name
 * @return Error code
 */
int plume_delete_string(char* str);





#if defined(__cplusplus)
}  // extern "C"
#endif
