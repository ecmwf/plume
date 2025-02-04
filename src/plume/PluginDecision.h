#pragma once

#include <iostream>
#include <vector>

#include "eckit/config/LocalConfiguration.h"

/**
 * @brief Class that stores the decision on whether to accept or reject a plugin
 *        and it also keeps track of the parameters actually offered to the plugin
 * 
 */
class PluginDecision {

    public:
    
        PluginDecision(bool accepted, const std::vector<std::string>& offeredParams = std::vector<std::string>()) :
            accepted_{accepted}, offeredParams_{offeredParams} {}
    
        bool accepted() const { return accepted_; }
    
        const std::vector<std::string>& offeredParams() const { return offeredParams_; }

        // print decision
        friend std::ostream& operator<<(std::ostream& os, const PluginDecision& decision) {
            os << "PluginDecision: " << (decision.accepted_ ? "ACCEPTED" : "REJECTED") << std::endl;
            os << "Agreed Parameters: [";
            if (decision.offeredParams_.size() == 0) {
                os << "]";
                return os;
            }

            for (int i = 0; i < decision.offeredParams_.size()-1; i++) {
                os << decision.offeredParams_[i] << ", ";
            }
            os << decision.offeredParams_[decision.offeredParams_.size()-1] << "]" << std::endl;
            return os;
        }
    
    private:
        bool accepted_;
        std::vector<std::string> offeredParams_;
};
    