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

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "eckit/config/LocalConfiguration.h"
#include "plume/PlumeTag.h"


namespace plume {

class PlumeState {

    // Type alias for a path in the execution tree (vector of child indices)
    using pathType = std::vector<std::size_t>;

public:
    static PlumeState& instance();

    /**
     * @brief Insert or update a state node and move current pointer to it.
     * For root-level state transitions (e.g. configure/negotiate/feed/teardown), call with parent unset.
     * For nested run transitions, pass the optional parent tag.
     * 
     * @param tag Current state tag.
     * @param parent Optional parent tag for nested run states.
     */
    void updateState(PlumeTag tag, std::optional<PlumeTag> parent = std::nullopt);

    void reset();

    eckit::LocalConfiguration getConfig() const;

    /**
     * @brief Getters for current node ("current" is here intended as "before next update")
     */
    std::string currentName() const;

    /**
     * @brief Getters for current node parent name
     */
    std::string currentParent() const;

    /**
     * @brief Getters for current node iteration count (absolute)
     */
    std::size_t currentIteration() const;

    /**
     * @brief Getters for current node iteration count relative to parent
     */
    std::size_t currentRelativeIteration() const;

    friend std::ostream& operator<<(std::ostream& os, const PlumeState& state);
    
private:

    struct TreeNode {
        PlumeTag name;
        std::size_t iteration{1};
        std::size_t iteration_relative{1};  ///< iteration count since parent's last update
        std::optional<PlumeTag> parent;
        std::vector<TreeNode> children;
    };

    // find a node in the execution tree by its path (vector of indices)
    // used to update the current node after each updateState call
    TreeNode* findTreeNodeByPath(const std::vector<std::size_t>& path);

    // const version of findTreeNodeByPath
    const TreeNode* findTreeNodeByPath(const pathType& path) const;

    // find the latest node with the given name in the execution tree,
    // (used to implement updateState when parent is specified)
    bool findLatestNodeByName(PlumeTag name, pathType& path) const;

    // recursive helper for findLatestNodeByName
    static bool findLatestNodeByNameRec(const std::vector<TreeNode>& nodes,
                                        PlumeTag name,
                                        pathType& prefix,
                                        pathType& foundPath,
                                        bool& found);

    // count how many nodes in the execution tree have a given name
    std::size_t countNodesByName(PlumeTag name) const;

    // recursive helper for countNodesByName
    static void countNodesByNameRec(const std::vector<TreeNode>& nodes,
                                    PlumeTag name,
                                    std::size_t& count);

    // Return pointer to current node, or nullptr if no current node.
    const TreeNode* currentNode() const;

    // Convert one TreeNode (recursively) into LocalConfiguration.
    static eckit::LocalConfiguration treeNodeToConfig(const TreeNode& node);

    // Update the exported configuration from the current execution tree and current node.
    // Marked as const since it does not modify the logical state, but only the cached configuration (mutable).
    void updateConfig() const;

    std::vector<TreeNode> executionTree_;
    std::optional<PlumeTag> current_;
    pathType currentPath_;
    mutable eckit::LocalConfiguration config_;
    mutable bool configDirty_{true};

};

std::ostream& operator<<(std::ostream& os, const PlumeState& state);

}  // namespace plume