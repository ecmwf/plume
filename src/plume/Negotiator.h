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

#include "Protocol.h"
#include "PluginDecision.h"
#include "eckit/config/LocalConfiguration.h"


namespace plume {

class Negotiator {
public:

    /**
     * @brief Negotiate with a plugin
     * 
     * @param requires 
     * @param config_params 
     * @return PluginDecision 
     */
    PluginDecision negotiate(const Protocol& offers,
                             const Protocol& requires,
                             const std::vector<eckit::LocalConfiguration>& config_params = std::vector<eckit::LocalConfiguration>{});
    
};


}  // namespace plume