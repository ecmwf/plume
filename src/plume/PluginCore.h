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

#include <iostream>
#include <map>
#include <optional>
#include <string>

#include "eckit/config/Configuration.h"
#include "eckit/exception/Exceptions.h"

#include "plume/data/ModelDataView.h"


namespace plume {

/**
 * @brief A plugincore that contains calculations/algorithms
 * to perform on model data. It is implemented inside a plugin
 * and loaded at runtime
 * 
 */
class PluginCore {

public:

    /**
     * @brief Construct a new PluginCore object
     * 
     * @param config 
     */
    PluginCore(const eckit::Configuration& config);

    /**
     * @brief Destroy the PluginCore object
     * 
     */
    virtual ~PluginCore();

    /**
     * @brief 
     * 
     * @return std::string 
     */
    std::string name() const;

    /**
     * @brief 
     * 
     * @return constexpr const char* 
     */
    constexpr static const char* type() { return "Base Runnable plugincore"; }

    /**
     * @brief Grab the necessary data that it needs from available data
     *
     * @param data  the plugin-facing view produced by ModelData::filter()
     */
    void grabData(data::ModelDataView data);

    /**
     * @brief Release the grabbed plugin-facing model data view at teardown.
     *
     * Called by PluginHandler::teardown(). The core is owned by the process-static PluginRegistry, so the
     * grabbed view (which shares the model's atlas::Field handles) must be dropped here — during the model's
     * finalise() while atlas is still alive — rather than at program exit when atlas' thread-local allocation
     * machinery has already been destroyed (which corrupts the heap).
     *
     * @note (PLUME-82) This explicit release is a consequence of the current design in which a ModelDataView
     *       *co-owns* the model's parameters (shared_ptr copies; see ModelData::filter and the ModelDataView
     *       class note). If views became non-owning observers of model parameters, plugin-view teardown would
     *       no longer destroy atlas::Field handles and this method would be unnecessary. PLUME-82 tracks that
     *       refactor.
     */
    void releaseData();

    /**
     * @brief Setup
     * 
     */
    virtual void setup(){
        // by default, do nothing
    };

    /**
     * @brief Teardown
     * 
     */
    virtual void teardown(){
        // by default, do nothing
    };

    /**
     * @brief Run
     * 
     */
    virtual void run() = 0;

protected:

    data::ModelDataView& modelData();

private:

    /** @brief Plugin-facing model data view.
     * 
     * Optional because ModelDataView has no default constructor — it only exists once filtered from real ModelData.
     * PluginCore is constructed before grabData() supplies the view, so the member starts empty and modelData() asserts
     * it is engaged, turning premature access into a clear error.
     */
    std::optional<data::ModelDataView> modelData_;
};


// fwd declaration
class PluginCoreBuilderBase;


// factory (registers/deregisters builders and calls "build")
class PluginCoreFactory {

public:  // methods
    static PluginCoreFactory& instance();

    void enregister(const std::string& name, const PluginCoreBuilderBase& builder);
    void deregister(const std::string& name);
    std::vector<std::string> list_registered();

    PluginCore* build(const std::string& name, const eckit::Configuration& config) const;

private:  // methods
    // Only one instance can be built, inside instance()
    PluginCoreFactory();
    ~PluginCoreFactory();

private:  // members
    mutable std::mutex mutex_;

    std::map<std::string, std::reference_wrapper<const PluginCoreBuilderBase>> builders_;
};


// base builder
class PluginCoreBuilderBase {
public:  // methods
    // Only instantiate from subclasses
    PluginCoreBuilderBase(const std::string& name);
    virtual ~PluginCoreBuilderBase();

    virtual PluginCore* make(const eckit::Configuration& config) const = 0;

public:  // members
    std::string name_;
};


// a concrete builder for a specific PluginCore type
template <typename T>
class PluginCoreBuilder : public PluginCoreBuilderBase {
public:  // methods
    // The name of the builder is taken from the type of the built object
    PluginCoreBuilder() : PluginCoreBuilderBase(T::type()) {}

    ~PluginCoreBuilder() override {}

    PluginCore* make(const eckit::Configuration& config) const override { return new T(config); }
};


}  // namespace plume
