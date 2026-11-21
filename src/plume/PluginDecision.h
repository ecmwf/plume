#pragma once

#include <iostream>
#include <map>
#include <set>
#include <vector>

#include <string>

#include "eckit/config/LocalConfiguration.h"

#include "plume/Hook.h"
#include "plume/data/ParameterCatalogue.h"

/**
 * @brief Class that stores the decision on whether to accept or reject a plugin,
 *        the parameters actually offered to the plugin, the hook points that
 *        the plugin has been accepted to run at, and the hook points at which
 *        each parameter is needed
 *
 */
class PluginDecision {

private:
    bool accepted_;
    std::set<plume::data::ParameterDefinition> offeredParams_;
    std::set<std::string> agreedHooks_;

    // param name -> hook points at which the param is needed (always a non-empty subset of agreedHooks_).
    // A param without an entry is needed at all of agreedHooks_.
    std::map<std::string, std::set<std::string>> paramHooks_;

    // collect the names of the params selected by "keep", mirroring offeredParamNames(derived)
    template <typename Pred>
    std::set<std::string> collectParamNames(bool derived, Pred keep) const {
        std::set<std::string> paramNames;
        for (const plume::data::ParameterDefinition& param : offeredParams_) {
            // includes params which are dependencies of observers if flag is turned on, else skip
            if (!derived && !param.sourceParam().empty()) {
                continue;
            }
            if (!keep(param)) {
                continue;
            }
            paramNames.insert(param.name());
            const auto& dependencies = param.dependencies();
            paramNames.insert(dependencies.begin(), dependencies.end());
        }
        return paramNames;
    }

public:
    PluginDecision(bool accepted, const std::set<plume::data::ParameterDefinition>& offeredParams = {},
                   const std::set<std::string>& agreedHooks                         = {plume::DEFAULT_HOOK},
                   const std::map<std::string, std::set<std::string>>& paramHooks = {}) :
        accepted_{accepted}, offeredParams_{offeredParams}, agreedHooks_{agreedHooks}, paramHooks_{paramHooks} {}

    bool accepted() const { return accepted_; }

    const std::set<std::string>& agreedHooks() const { return agreedHooks_; }

    const std::set<plume::data::ParameterDefinition>& offeredParams() const { return offeredParams_; }

    /**
     * @brief The hook points at which a param is needed (all the agreed hook points if it has no specific entry)
     */
    const std::set<std::string>& hooksOf(const std::string& name) const {
        auto it = paramHooks_.find(name);
        return (it == paramHooks_.end()) ? agreedHooks_ : it->second;
    }

    const std::set<std::string> offeredParamNames(bool derived = true) const {
        return collectParamNames(derived, [](const plume::data::ParameterDefinition&) { return true; });
    }

    /**
     * @brief As offeredParamNames(), restricted to the params needed at a given hook point.
     *
     * The dependencies of a derived param are reported at the hook points where the derived param is needed.
     * The union over agreedHooks() equals offeredParamNames(derived).
     */
    std::set<std::string> offeredParamNamesAtHook(const std::string& hook, bool derived = true) const {
        return collectParamNames(derived, [this, &hook](const plume::data::ParameterDefinition& param) {
            const auto& hooks = hooksOf(param.name());
            return hooks.find(hook) != hooks.end();
        });
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

        // only the params that are not needed at every agreed hook point
        bool anyScoped = false;
        for (const auto& [name, hooks] : decision.paramHooks_) {
            if (hooks == decision.agreedHooks_) {
                continue;
            }
            os << (anyScoped ? ", " : "Hook-scoped Parameters: [") << name << "@{";
            for (auto it = hooks.begin(); it != hooks.end(); ++it) {
                if (it != hooks.begin())
                    os << ", ";
                os << *it;
            }
            os << "}";
            anyScoped = true;
        }
        if (anyScoped) {
            os << "]" << std::endl;
        }
        return os;
    }
};
