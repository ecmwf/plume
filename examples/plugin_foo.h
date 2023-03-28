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
#include <string>
#include "plume/Kernel.h"
#include "plume/Plugin.h"

namespace plugin_foo {


/**
 * @brief Kernel simple example
 * 
 */
class KernelFoo : public plume::Kernel {
public:
    KernelFoo(const eckit::Configuration& conf);
    ~KernelFoo();
    void run() override;
    constexpr static const char* type() { return "kernel-foo"; }
};


/**
 * @brief Plugin simple example
 * 
 */
class PluginFoo : public plume::Plugin {

public:
    PluginFoo();

    ~PluginFoo();

    plume::Protocol negotiate() override {
        plume::Protocol protocol;
        protocol.requireInt("I");
        protocol.requireInt("J");
        return protocol;
    }

    static const PluginFoo& instance();

    std::string version() const override { return "0.0.1"; }

    std::string gitsha1(unsigned int count) const override { return "undefined"; }

    virtual std::string kernelName() const override { return KernelFoo::type(); }
};


}  // namespace plugin_foo
