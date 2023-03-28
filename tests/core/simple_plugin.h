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

namespace plume_example_plugin {


// ------ Foo runnable kernel that self-registers! -------
class SimpleKernel : public plume::Kernel {
public:
    SimpleKernel(const eckit::Configuration& conf);
    ~SimpleKernel();
    void run() override;
    constexpr static const char* type() { return "simple-kernel"; }
};
// ------------------------------------------------------

// ------------------------------------------------------
class SimplePlugin : public plume::Plugin {

public:
    SimplePlugin();

    ~SimplePlugin();

    plume::Protocol negotiate() override {
        plume::Protocol protocol;
        protocol.requireInt("I");
        protocol.requireInt("J");
        protocol.requireInt("K");

        return protocol;
    }

    // Return the static instance
    static const SimplePlugin& instance();

    std::string version() const override { return "0.0.1-Simple"; }

    std::string gitsha1(unsigned int count) const override { return "undefined"; }

    virtual std::string kernelName() const override { return SimpleKernel::type(); }
};
// ------------------------------------------------------

}  // namespace plume_example_plugin
