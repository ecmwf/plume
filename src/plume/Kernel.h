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
#include <string>

#include "eckit/config/Configuration.h"
#include "eckit/exception/Exceptions.h"

#include "plume/data/ModelData.h"


namespace plume {

/**
 * @brief A kernel that contains calculations/algorithms
 * to perform on model data. It is implemented inside a plugin
 * and loaded at runtime
 * 
 */
class Kernel {

public:

    /**
     * @brief Construct a new Kernel object
     * 
     * @param config 
     */
    Kernel(const eckit::Configuration& config);

    /**
     * @brief Destroy the Kernel object
     * 
     */
    virtual ~Kernel();

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
    constexpr static const char* type() { return "Base Runnable kernel"; }

    /**
     * @brief Grab the necessary data that it needs from available data
     * 
     * @param data 
     */
    void grabData(const data::ModelData& data);

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

    data::ModelData& modelData();

private:

    data::ModelData modelData_;
};


// fwd declaration
class KernelBuilderBase;


// factory (registers/deregisters builders and calls "build")
class KernelFactory {

public:  // methods
    static KernelFactory& instance();

    void enregister(const std::string& name, const KernelBuilderBase& builder);
    void deregister(const std::string& name);
    std::vector<std::string> list_registered();

    Kernel* build(const std::string& name, const eckit::Configuration& config) const;

private:  // methods
    // Only one instance can be built, inside instance()
    KernelFactory();
    ~KernelFactory();

private:  // members
    mutable std::mutex mutex_;

    std::map<std::string, std::reference_wrapper<const KernelBuilderBase>> builders_;
};


// base builder
class KernelBuilderBase {
public:  // methods
    // Only instantiate from subclasses
    KernelBuilderBase(const std::string& name);
    virtual ~KernelBuilderBase();

    virtual Kernel* make(const eckit::Configuration& config) const = 0;

public:  // members
    std::string name_;
};


// a concrete builder for a specific Kernel type
template <typename T>
class KernelBuilder : public KernelBuilderBase {
public:  // methods
    // The name of the builder is taken from the type of the built object
    KernelBuilder() : KernelBuilderBase(T::type()) {}

    ~KernelBuilder() override {}

    Kernel* make(const eckit::Configuration& config) const override { return new T(config); }
};


}  // namespace plume
