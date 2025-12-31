#pragma once

#include <iostream>
#include <set>
#include <vector>

#include "eckit/config/LocalConfiguration.h"

#include "plume/data/ParameterCatalogue.h"

/**
 * @brief Class that stores the decision on whether to accept or reject a plugin
 *        and it also keeps track of the parameters actually offered to the plugin
 *
 */
class PluginDecision {

private:
    bool accepted_;
    std::set<plume::data::ParameterDefinition> offeredParams_;

public:
    PluginDecision(bool accepted, const std::set<plume::data::ParameterDefinition>& offeredParams = {}) :
        accepted_{accepted}, offeredParams_{offeredParams} {}

    bool accepted() const { return accepted_; }

    const std::set<plume::data::ParameterDefinition>& offeredParams() const { return offeredParams_; }
    const std::set<std::string> offeredParamNames(bool derived = true) const {
        std::set<std::string> paramNames;
        for (const plume::data::ParameterDefinition& param : offeredParams_) {
            // includes params which are dependencies of observers if flag is turned on, else skip
            if (!derived && !param.sourceParam().empty()) {
                continue;
            }
            paramNames.insert(param.name());
            const auto& dependencies = param.dependencies();
            paramNames.insert(dependencies.begin(), dependencies.end());
        }
        return paramNames;
    }

    // print decision
    friend std::ostream& operator<<(std::ostream& os, const PluginDecision& decision) {
        os << "PluginDecision: " << (decision.accepted_ ? "ACCEPTED\n" : "REJECTED\n");
        os << "Agreed Parameters: [";
        if (decision.offeredParams_.size() == 0) {
            os << "]";
            return os;
        }

        for (auto it = decision.offeredParams_.begin(); it != decision.offeredParams_.end(); ++it) {
            if (it != decision.offeredParams_.begin())
                os << ", ";
            os << *it;
        }
        os << "]" << std::endl;
        return os;
    }
};
