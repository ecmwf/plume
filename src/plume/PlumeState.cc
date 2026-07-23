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
#include "plume/PlumeState.h"
#include "plume/PlumeTag.h"
#include "plume/utils.h"


#include <algorithm>
#include <iostream>
#include <sstream>

#include "eckit/log/Log.h"
#include "eckit/exception/Exceptions.h"

namespace plume {


// Return the singleton state object.
PlumeState& PlumeState::instance() {
    static PlumeState plumeState;
    return plumeState;
}

// Insert or update a state node and move current pointer to it.
void PlumeState::updateState(PlumeTag tag, std::optional<PlumeTag> parent) {

    // Starting a configure step always begins a fresh execution workflow.
    if (tag == PlumeTag::CONFIGURE && !parent) {
        executionTree_.clear();
        currentPath_.clear();
    }

    pathType parentPath;

    // Siblings to search in: root nodes by default.
    std::vector<TreeNode>* siblings = &executionTree_;

    // If caller specified a parent tag, resolve the latest matching node.
    std::optional<PlumeTag> parentName;
    if (parent) {

        const std::size_t parentMatches = countNodesByName(*parent);
        if (parentMatches > 1) {
            std::string errorMsg = "Ambiguous parent tag in execution tree: " + plumeTagToString(*parent) +
                                   ". Use unique parent tags in client code.";
            throw eckit::BadParameter(errorMsg, Here());
        }

        // Find path of latest node named 'parent'.
        if (!findLatestNodeByName(*parent, parentPath)) {
            // Parent was requested but does not exist.
            std::string errorMsg = "Parent not found in execution tree: " + plumeTagToString(*parent);
            throw eckit::BadParameter(errorMsg, Here());
        }

        // Resolve (and check) node pointer from computed path.
        TreeNode* parentNode = findTreeNodeByPath(parentPath);
        if (!parentNode) {
            std::string errorMsg = "Parent path not found in execution tree for parent: " + plumeTagToString(*parent);
            throw eckit::BadParameter(errorMsg, Here());
        }

        parentName = parentNode->name; // Use resolved parent node metadata
        siblings = &parentNode->children; // Children become target sibling list
    }

    // Find existing node in sibling set matching both tag and parent name.
    auto treeNodeIt = std::find_if(siblings->begin(), siblings->end(), [&tag, &parentName](const TreeNode& node) {
        return node.name == tag && node.parent == parentName;
    });

    // Index of the node we will end up selecting/creating.
    std::size_t nodeIndex = 0;
    // Existing node case: increment counters.
    if (treeNodeIt != siblings->end()) {
        treeNodeIt->iteration += 1;
        treeNodeIt->iteration_relative += 1;
        // Parent iteration moved, so each direct child starts a new relative epoch.
        for (auto& child : treeNodeIt->children) {
            child.iteration_relative = 0;
        }
        // Capture index of existing node.
        nodeIndex = static_cast<std::size_t>(std::distance(siblings->begin(), treeNodeIt));
    }
    // New node case: insert with initial counters set to 1.
    else {
        // Insert node with resolved parent name and empty child list.
        siblings->push_back({tag, 1, 1, parentName, {}});
        // New node is at the back.
        nodeIndex = siblings->size() - 1;
    }

    // Set Current path (parent path first, then append selected child index).
    currentPath_ = parentPath;
    currentPath_.push_back(nodeIndex);

    // Cache current node name for quick access.
    current_ = tag;
    // Mark serialized state snapshot dirty; it will be rebuilt lazily on read.
    configDirty_ = true;
}

// Fully clear runtime state and publish empty config.
void PlumeState::reset() {
    executionTree_.clear();
    current_.reset();
    currentPath_.clear();
    configDirty_ = true;
}

// Return a copy of the exported configuration snapshot.
eckit::LocalConfiguration PlumeState::getConfig() const {
    if (configDirty_) {
        updateConfig();
    }
    return config_;
}

// Return the current node name (empty if no current node).
std::string PlumeState::currentName() const {
    return current_ ? plumeTagToString(*current_) : "";
}

// Return current node parent name (empty for root/no-current).
std::string PlumeState::currentParent() const {
    const TreeNode* node = currentNode();
    // If current exists return parent name, otherwise empty string.
    return (node && node->parent) ? plumeTagToString(*node->parent) : "";
}

// Return absolute iteration count for current node.
std::size_t PlumeState::currentIteration() const {
    const TreeNode* node = currentNode();
    // If current exists return absolute iteration, otherwise 0.
    return node ? node->iteration : 0;
}

// Return iteration count relative to parent epoch for current node.
std::size_t PlumeState::currentRelativeIteration() const {
    const TreeNode* node = currentNode();
    // If current exists return relative iteration, otherwise 0.
    return node ? node->iteration_relative : 0;
}

// Stream state as JSON-like text.
std::ostream& operator<<(std::ostream& os, const PlumeState& state) {
    auto cfg = state.getConfig();
    configToJson(cfg, os, 2);
    return os;
}


// ----------- private methods -----------

// Resolve mutable node pointer from index path.
PlumeState::TreeNode* PlumeState::findTreeNodeByPath(const pathType& path) {
    // Empty path means "no node".
    if (path.empty()) {
        return nullptr;
    }

    // Start traversal at root sibling vector.
    std::vector<TreeNode>* siblings = &executionTree_;
    // Track current node as we descend.
    TreeNode* currentNode           = nullptr;

    // Walk each index in the path.
    for (const auto& idx : path) {
        // Guard against invalid index.
        if (idx >= siblings->size()) {
            return nullptr;
        }

        currentNode = &(*siblings)[idx]; // Step into selected sibling.
        siblings = &currentNode->children; // Next indices refer to its children.
    }

    // Return resolved node pointer.
    return currentNode;
}

// Resolve const node pointer from index path.
const PlumeState::TreeNode* PlumeState::findTreeNodeByPath(const pathType& path) const {
    // Empty path means "no node".
    if (path.empty()) {
        return nullptr;
    }

    // Start traversal at root sibling vector.
    const std::vector<TreeNode>* siblings = &executionTree_;
    // Track current node as we descend.
    const TreeNode* currentNode = nullptr;

    // Walk each index in the path.
    for (const auto& idx : path) {
        // Guard against invalid index.
        if (idx >= siblings->size()) {
            return nullptr;
        }

        // Step into selected sibling.
        currentNode = &(*siblings)[idx];
        // Next indices refer to its children.
        siblings = &currentNode->children;
    }

    // Return resolved node pointer.
    return currentNode;
}

// Find path of the latest node matching a name using depth-first traversal.
bool PlumeState::findLatestNodeByName(PlumeTag name, pathType& path) const {

    pathType prefix; // Current DFS traversal prefix path.
    pathType foundPath; // Last matching path found by DFS.
    bool found = false; // "found anything" flag shared with recursion.

    // Recursive DFS over full tree.
    findLatestNodeByNameRec(executionTree_, name, prefix, foundPath, found);
    // If at least one match was found, publish the latest path.
    if (found) {
        path = foundPath;
    }
    // Return whether a match exists.
    return found;
}

std::size_t PlumeState::countNodesByName(PlumeTag name) const {
    std::size_t count = 0;
    countNodesByNameRec(executionTree_, name, count);
    return count;
}

void PlumeState::countNodesByNameRec(const std::vector<TreeNode>& nodes,
                                     PlumeTag name,
                                     std::size_t& count) {
    for (const auto& node : nodes) {
        if (node.name == name) {
            ++count;
        }
        countNodesByNameRec(node.children, name, count);
    }
}

// DFS helper: updates foundPath whenever it visits a matching node.
bool PlumeState::findLatestNodeByNameRec(const std::vector<TreeNode>& nodes,
                                         PlumeTag name,
                                         std::vector<std::size_t>& prefix,
                                         std::vector<std::size_t>& foundPath,
                                         bool& found) {
    // Iterate siblings in order.
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        // Enter node i by extending prefix.
        prefix.push_back(i);

        // If node name matches target, remember this path as latest so far.
        if (nodes[i].name == name) {
            foundPath = prefix;
            found = true;
        }

        // Recurse into children (depth-first).
        findLatestNodeByNameRec(nodes[i].children, name, prefix, foundPath, found);
        // Leave node i and restore prefix for next sibling.
        prefix.pop_back();
    }

    // Return whether any match has been seen in this DFS.
    return found;
}

// Resolve pointer to current node from currentPath_.
const PlumeState::TreeNode* PlumeState::currentNode() const {
    return findTreeNodeByPath(currentPath_);
}

// Convert one TreeNode (recursively) into LocalConfiguration.
eckit::LocalConfiguration PlumeState::treeNodeToConfig(const TreeNode& node) {

    eckit::LocalConfiguration nodeConfig;

    nodeConfig.set("name", plumeTagToString(node.name));
    nodeConfig.set("iteration", static_cast<size_t>(node.iteration));
    nodeConfig.set("iteration_relative", static_cast<size_t>(node.iteration_relative));

    // Store parent field (empty string for root nodes).
    if (node.parent) {
        nodeConfig.set("parent", plumeTagToString(*node.parent));
    } else {
        nodeConfig.set("parent", "");
    }

    // Recursively serialize children if present.
    if (!node.children.empty()) {
        std::vector<eckit::LocalConfiguration> childrenConfig;
        childrenConfig.reserve(node.children.size());
        for (const auto& child : node.children) {
            childrenConfig.push_back(treeNodeToConfig(child));
        }
        nodeConfig.set("children", childrenConfig);
    }

    // Return serialized node.
    return nodeConfig;
}


// Rebuild exported LocalConfiguration snapshot from in-memory tree.
void PlumeState::updateConfig() const {

    eckit::LocalConfiguration state;
    std::vector<eckit::LocalConfiguration> treeConfig;
    treeConfig.reserve(executionTree_.size());

    // Recursively serialize each node.
    for (const auto& node : executionTree_) {
        treeConfig.push_back(treeNodeToConfig(node));
    }

    state.set("execution_tree", treeConfig);
    state.set("current", current_ ? plumeTagToString(*current_) : "");

    // Optionally publish full current node payload.
    const TreeNode* node = currentNode();
    if (node) {
        state.set("current_node", treeNodeToConfig(*node));
    }

    // Atomically replace cached config snapshot.
    config_ = state;
    configDirty_ = false;
}

}  // namespace plume