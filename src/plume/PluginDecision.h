#pragma once

#include <iostream>
#include <set>
#include <vector>

#include <string>

#include "eckit/config/LocalConfiguration.h"

#include "plume/Hook.h"
#include "plume/data/ParameterCatalogue.h"

/**
 * @brief Class that stores the decision on whether to accept or reject a plugin,
 *        the parameters actually offered to the plugin, and the hook points that
 *        the plugin has been accepted to run at
 *
 */
class PluginDecision {

private:
    bool accepted_;
    std::set<plume::data::ParameterDefinition> offeredParams_;
    std::set<std::string> agreedHooks_;

public:
    PluginDecision(bool accepted, const std::set<plume::data::ParameterDefinition>& offeredParams = {},
                   const std::set<std::string>& agreedHooks = {plume::DEFAULT_HOOK}) :
        accepted_{accepted}, offeredParams_{offeredParams}, agreedHooks_{agreedHooks} {}

    bool accepted() const { return accepted_; }

    const std::set<std::string>& agreedHooks() const { return agreedHooks_; }

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

        os << "Agreed Hook Points: [";
        for (auto it = decision.agreedHooks_.begin(); it != decision.agreedHooks_.end(); ++it) {
            if (it != decision.agreedHooks_.begin())
                os << ", ";
            os << *it;
        }
        os << "]" << std::endl;

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
