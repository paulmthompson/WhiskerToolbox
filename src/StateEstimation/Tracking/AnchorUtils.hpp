#ifndef STATEESTIMATION_TRACKING_ANCHOR_UTILS_HPP
#define STATEESTIMATION_TRACKING_ANCHOR_UTILS_HPP

#include "Tracking/Tracklet.hpp"
#include "Entity/EntityGroupManager.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace StateEstimation {

/**
     * @brief Locate anchors with both meta-node and member indices.
     *
     * @return Optional positions if both anchors are found.
     */
struct AnchorPositions {
    int start_meta_index = -1;
    size_t start_member_index = 0;
    int end_meta_index = -1;
    size_t end_member_index = 0;
};

// Arc metadata: stores the actual chain of entities represented by this arc
struct ArcChain {
    std::vector<NodeInfo> entities;// All entities along this arc (including endpoints)
    int64_t cost;
};

// --- Helper Functions for Ground Truth Processing ---

/**
     * @brief Structure to hold ground truth anchor pairs for a group.
     * Each pair represents consecutive ground truth labels for a group.
     */
struct GroundTruthSegment {
    GroupId group_id;
    TimeFrameIndex start_frame = TimeFrameIndex(0);
    EntityId start_entity;
    TimeFrameIndex end_frame = TimeFrameIndex(0);
    EntityId end_entity;
};

using GroundTruthMap = std::map<TimeFrameIndex, std::map<GroupId, EntityId>>;

/**
 * @brief Locate positions of start and end anchors within a collection of meta-nodes.
 *
 * Searches for the first occurrences of (start_frame,start_entity) and
 * (end_frame,end_entity) inside `meta_nodes` and returns both the meta-node index
 * and the member index for each anchor when found.
 *
 * @return std::optional of tuple (start_meta_index, start_member_index, end_meta_index, end_member_index)
 */
inline std::optional<std::tuple<int, std::size_t, int, std::size_t>>
findAnchorPositionsInMetaNodes(std::vector<MetaNode> const & meta_nodes,
                               TimeFrameIndex start_frame,
                               EntityId start_entity,
                               TimeFrameIndex end_frame,
                               EntityId end_entity) {
    int start_meta_index = -1;
    std::size_t start_member_index = 0;
    int end_meta_index = -1;
    std::size_t end_member_index = 0;

    for (std::size_t i = 0; i < meta_nodes.size(); ++i) {
        auto const & mn = meta_nodes[i];
        for (std::size_t k = 0; k < mn.members.size(); ++k) {
            auto const & m = mn.members[k];
            if (start_meta_index == -1 && m.frame == start_frame && m.entity_id == start_entity) {
                start_meta_index = static_cast<int>(i);
                start_member_index = k;
            }
            if (end_meta_index == -1 && m.frame == end_frame && m.entity_id == end_entity) {
                end_meta_index = static_cast<int>(i);
                end_member_index = k;
            }
        }
        if (start_meta_index != -1 && end_meta_index != -1) break;
    }

    if (start_meta_index == -1 || end_meta_index == -1) {
        return std::nullopt;
    }
    return std::make_optional(std::make_tuple(start_meta_index, start_member_index,
                                              end_meta_index, end_member_index));
}

/**
 * @brief Returns AnchorPositions (meta-node and member indices) for the two anchors.
 */
inline std::optional<AnchorPositions>
findAnchorPositions(std::vector<MetaNode> const & meta_nodes,
                    TimeFrameIndex start_frame,
                    EntityId start_entity,
                    TimeFrameIndex end_frame,
                    EntityId end_entity) {
    auto pos_opt = findAnchorPositionsInMetaNodes(meta_nodes, start_frame, start_entity, end_frame, end_entity);
    if (!pos_opt) return std::nullopt;
    auto const & [smi, sidx, emi, eidx] = *pos_opt;
    AnchorPositions positions;
    positions.start_meta_index = smi;
    positions.start_member_index = sidx;
    positions.end_meta_index = emi;
    positions.end_member_index = eidx;
    return positions;
}

/**
 * @brief Convenience that returns only the meta-node indices that contain the two anchors.
 * Returns {-1,-1} if either anchor is not found.
 */
inline std::pair<int, int>
findAnchorMetaNodeIndices(std::vector<MetaNode> const & meta_nodes,
                          TimeFrameIndex start_frame,
                          EntityId start_entity,
                          TimeFrameIndex end_frame,
                          EntityId end_entity) {
    auto pos_opt = findAnchorPositionsInMetaNodes(meta_nodes, start_frame, start_entity, end_frame, end_entity);
    if (!pos_opt) return {-1, -1};
    auto const & [smi, /*sidx*/ _, emi, /*eidx*/ __] = *pos_opt;
    return {smi, emi};
}

/**
     * @brief Convert ground truth map into segments for each group.
     * 
     * Groups ground truth labels by GroupId and creates consecutive pairs.
     * For example, if group 1 has labels at frames 1, 1000, 5000, this creates:
     * - Segment 1: (1, entity1) -> (1000, entity1000)
     * - Segment 2: (1000, entity1000) -> (5000, entity5000)
     * 
     * @param ground_truth Map of frame -> (group_id -> entity_id)
     * @return Vector of ground truth segments, ordered by group and frame
     */
inline std::vector<GroundTruthSegment> extractGroundTruthSegments(GroundTruthMap const & ground_truth) {
    std::vector<GroundTruthSegment> segments;

    // Group ground truth by GroupId
    std::map<GroupId, std::vector<std::pair<TimeFrameIndex, EntityId>>> group_anchors;

    for (auto const & [frame, group_entities]: ground_truth) {
        for (auto const & [group_id, entity_id]: group_entities) {
            group_anchors[group_id].emplace_back(frame, entity_id);
        }
    }

    // Sort each group's anchors by frame and create consecutive pairs
    for (auto & [group_id, anchors]: group_anchors) {
        // Sort by frame index
        std::sort(anchors.begin(), anchors.end(),
                  [](auto const & a, auto const & b) { return a.first < b.first; });

        // Create consecutive pairs
        for (size_t i = 0; i < anchors.size() - 1; ++i) {
            // Only produce segments when there are unlabeled frames between anchors
            // i.e., skip pairs on consecutive frames (no work to assign)
            int const gap = (anchors[i + 1].first - anchors[i].first).getValue();
            if (gap <= 1) continue;

            GroundTruthSegment segment;
            segment.group_id = group_id;
            segment.start_frame = anchors[i].first;
            segment.start_entity = anchors[i].second;
            segment.end_frame = anchors[i + 1].first;
            segment.end_entity = anchors[i + 1].second;
            segments.push_back(segment);
        }
    }

    return segments;
}


/**
     * @brief Find meta-nodes that contain the specified ground truth anchors.
     * 
     * Searches through meta-nodes to find those that contain the start and end
     * entities at the specified frames for a ground truth segment.
     * 
     * @param meta_nodes Vector of meta-nodes to search
     * @param segment Ground truth segment containing start/end anchors
     * @return Pair of (start_meta_index, end_meta_index) or (-1, -1) if not found
     */
inline std::pair<int, int> findAnchorMetaNodes(
        std::vector<MetaNode> const & meta_nodes,
        GroundTruthSegment const & segment) {
    auto pair_indices = findAnchorMetaNodeIndices(meta_nodes,
                                                  segment.start_frame,
                                                  segment.start_entity,
                                                  segment.end_frame,
                                                  segment.end_entity);
    return pair_indices;
}


inline std::optional<AnchorPositions> findAnchorPositions(
        std::vector<MetaNode> const & meta_nodes,
        GroundTruthSegment const & segment) {
    return StateEstimation::findAnchorPositions(meta_nodes,
                                                segment.start_frame,
                                                segment.start_entity,
                                                segment.end_frame,
                                                segment.end_entity);
}


/**
     * @brief Create a trimmed copy of meta-nodes restricted to a ground-truth segment.
     *
     * Keeps only meta-nodes that lie strictly within (start_frame, end_frame) and the two
     * boundary meta-nodes that contain the anchors. Boundary meta-nodes are spliced so that
     * their start/end align exactly with the anchor frames. Any other meta-node that crosses
     * a boundary without containing the corresponding anchor is discarded.
     *
     * @param meta_nodes All meta-nodes across the full range
     * @param segment Ground truth segment specifying [start_frame, end_frame] and entities
     * @return Trimmed meta-nodes for the segment. Empty if anchors are not found.
     */
inline std::vector<MetaNode> sliceMetaNodesToSegment(
        std::vector<MetaNode> const & meta_nodes,
        GroundTruthSegment const & segment) {
    auto const positions_opt = findAnchorPositions(meta_nodes, segment);
    if (!positions_opt.has_value()) {
        // Anchors not found
        return {};
    }
    auto const start_meta_index = positions_opt->start_meta_index;
    auto const start_member_index = positions_opt->start_member_index;
    auto const end_meta_index = positions_opt->end_meta_index;
    auto const end_member_index = positions_opt->end_member_index;

    // Special case: both anchors lie within the same meta-node
    if (start_meta_index == end_meta_index) {
        std::vector<MetaNode> result;
        MetaNode trimmed;
        auto const & src = meta_nodes[static_cast<size_t>(start_meta_index)];
        // Extract inclusive slice between the two anchor members
        if (start_member_index <= end_member_index) {
            for (size_t k = start_member_index; k <= end_member_index && k < src.members.size(); ++k) {
                trimmed.members.push_back(src.members[k]);
            }
        } else {
            // Degenerate/invalid ordering; return empty
            return {};
        }
        if (!trimmed.members.empty()) {
            trimmed.start_frame = trimmed.members.front().frame;
            trimmed.start_entity = trimmed.members.front().entity_id;
            trimmed.end_frame = trimmed.members.back().frame;
            trimmed.end_entity = trimmed.members.back().entity_id;
            // Keep states from original node (approximation consistent with other trims)
            trimmed.start_state = src.start_state;
            trimmed.end_state = src.end_state;
            result.push_back(std::move(trimmed));
        }
        return result;
    }

    // General case: anchors are in different meta-nodes
    std::vector<MetaNode> output;
    output.reserve(meta_nodes.size());

    for (size_t i = 0; i < meta_nodes.size(); ++i) {
        auto const & mn = meta_nodes[i];

        // Completely outside the segment range
        //if (mn.end_frame <= segment.start_frame || mn.start_frame >= segment.end_frame) {
        if (mn.end_frame < segment.start_frame || mn.start_frame > segment.end_frame) {
            continue;
        }

        if (static_cast<int>(i) == start_meta_index) {
            // Splice suffix starting at the start anchor member
            MetaNode trimmed = mn;
            std::vector<NodeInfo> members;
            for (size_t k = start_member_index; k < mn.members.size(); ++k) {
                // Keep only until strictly before end_frame to avoid leaking past boundary
                //if (mn.members[k].frame > segment.end_frame) break;
                if (mn.members[k].frame >= segment.end_frame) break;
                members.push_back(mn.members[k]);
            }
            if (members.empty()) continue;
            trimmed.members = std::move(members);
            trimmed.start_frame = trimmed.members.front().frame;
            trimmed.start_entity = trimmed.members.front().entity_id;
            // Preserve original end state (approximation)
            trimmed.end_frame = trimmed.members.back().frame;
            trimmed.end_entity = trimmed.members.back().entity_id;
            output.push_back(std::move(trimmed));
            continue;
        }

        if (static_cast<int>(i) == end_meta_index) {
            // Splice prefix ending at the end anchor member; include all frames after start_frame up to end anchor
            MetaNode trimmed = mn;
            std::vector<NodeInfo> members;
            for (size_t k = 0; k <= end_member_index && k < mn.members.size(); ++k) {
                if (mn.members[k].frame <= segment.start_frame) continue;
                members.push_back(mn.members[k]);
            }
            if (members.empty()) continue;
            trimmed.members = std::move(members);
            trimmed.start_frame = trimmed.members.front().frame;
            trimmed.start_entity = trimmed.members.front().entity_id;
            trimmed.end_frame = trimmed.members.back().frame;
            trimmed.end_entity = trimmed.members.back().entity_id;
            output.push_back(std::move(trimmed));
            continue;
        }

        // For interior nodes, keep only those strictly within (start_frame, end_frame)
        if (mn.start_frame > segment.start_frame && mn.end_frame < segment.end_frame) {
            output.push_back(mn);
        } else {
            // Discard nodes that cross boundaries without containing anchors
            continue;
        }
    }

    return output;
}

/**
 * @brief Build a simple fallback path by concatenating members of start and end meta-nodes.
 *
 * Assumes the input meta-nodes are already trimmed to the segment using sliceMetaNodesToSegment.
 * If start and end refer to the same meta-node, returns its members. Otherwise, concatenates
 * all members from the start meta-node followed by all members from the end meta-node.
 */
inline Path buildFallbackPathFromTrimmed(std::vector<MetaNode> const & meta_nodes_trimmed,
                                         int start_meta_index,
                                         int end_meta_index) {
    Path fallback_path;
    int const num_meta = static_cast<int>(meta_nodes_trimmed.size());
    if (start_meta_index >= 0 && start_meta_index < num_meta) {
        auto const & start_node_members = meta_nodes_trimmed[static_cast<size_t>(start_meta_index)].members;
        fallback_path.insert(fallback_path.end(), start_node_members.begin(), start_node_members.end());
    }
    if (end_meta_index >= 0 && end_meta_index < num_meta && end_meta_index != start_meta_index) {
        auto const & end_node_members = meta_nodes_trimmed[static_cast<size_t>(end_meta_index)].members;
        fallback_path.insert(fallback_path.end(), end_node_members.begin(), end_node_members.end());
    }
    return fallback_path;
}

}// namespace StateEstimation

#endif// STATEESTIMATION_TRACKING_ANCHOR_UTILS_HPP
