#include <catch2/catch_test_macros.hpp>

#include "Tracking/Tracklet.hpp"
#include "Tracking/AnchorUtils.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "Entity/EntityTypes.hpp"

using namespace StateEstimation;

namespace {

MetaNode makeMetaNode(std::vector<std::pair<long long, EntityId>> const & frame_entity_pairs) {
    MetaNode mn;
    mn.members.reserve(frame_entity_pairs.size());
    for (auto const & fe : frame_entity_pairs) {
        mn.members.push_back(NodeInfo{TimeFrameIndex(static_cast<int>(fe.first)), fe.second});
    }
    if (!mn.members.empty()) {
        mn.start_frame = mn.members.front().frame;
        mn.end_frame = mn.members.back().frame;
        mn.start_entity = mn.members.front().entity_id;
        mn.end_entity = mn.members.back().entity_id;
    }
    return mn;
}

}

TEST_CASE("StateEstimation - MinCostFlowTracker - findAnchorMetaNodeIndices finds correct meta-node indices across nodes", "[MinCostFlowTracker][anchors]") {
    // Build mock meta-nodes across frames with entity labels
    // Meta 0: frames 10,11,12 entity ids 1000,1001,1002
    // Meta 1: frames 13,14 entity ids 2000,2001
    // Meta 2: frames 15,16,17 entity ids 3000,3001,3002
    std::vector<MetaNode> meta_nodes;
    meta_nodes.push_back(makeMetaNode({{10, 1000}, {11, 1001}, {12, 1002}}));
    meta_nodes.push_back(makeMetaNode({{13, 2000}, {14, 2001}}));
    meta_nodes.push_back(makeMetaNode({{15, 3000}, {16, 3001}, {17, 3002}}));

    // Start anchor at (frame 11, entity 1001) inside meta 0
    // End anchor at (frame 16, entity 3001) inside meta 2
    auto const [start_idx, end_idx] = findAnchorMetaNodeIndices(
        meta_nodes, TimeFrameIndex(11), static_cast<EntityId>(1001), TimeFrameIndex(16), static_cast<EntityId>(3001));

    REQUIRE(start_idx == 0);
    REQUIRE(end_idx == 2);
}

TEST_CASE("StateEstimation - MinCostFlowTracker - findAnchorPositionsInMetaNodes returns member indices for anchors within a single meta-node", "[MinCostFlowTracker][anchors]") {
    // Single meta-node containing both anchors
    // frames: 20,21,22,23; entity ids 5000,5001,5002,5003
    std::vector<MetaNode> meta_nodes;
    meta_nodes.push_back(makeMetaNode({{20, 5000}, {21, 5001}, {22, 5002}, {23, 5003}}));

    auto opt = findAnchorPositionsInMetaNodes(meta_nodes,
                                              TimeFrameIndex(21), static_cast<EntityId>(5001),
                                              TimeFrameIndex(23), static_cast<EntityId>(5003));
    REQUIRE(opt.has_value());
    auto const & [start_meta, start_member, end_meta, end_member] = *opt;
    REQUIRE(start_meta == 0);
    REQUIRE(end_meta == 0);
    REQUIRE(start_member == 1);
    REQUIRE(end_member == 3);
}

TEST_CASE("StateEstimation - MinCostFlowTracker - findAnchorPositionsInMetaNodes returns nullopt when either anchor is missing", "[MinCostFlowTracker][anchors]") {
    std::vector<MetaNode> meta_nodes;
    meta_nodes.push_back(makeMetaNode({{30, 7000}, {31, 7001}}));
    meta_nodes.push_back(makeMetaNode({{32, 8000}}));

    // Missing end anchor
    auto opt1 = findAnchorPositionsInMetaNodes(meta_nodes, TimeFrameIndex(31), static_cast<EntityId>(7001),
                                               TimeFrameIndex(40), static_cast<EntityId>(9000));
    REQUIRE_FALSE(opt1.has_value());

    // Missing start anchor
    auto opt2 = findAnchorPositionsInMetaNodes(meta_nodes, TimeFrameIndex(29), static_cast<EntityId>(6000),
                                               TimeFrameIndex(32), static_cast<EntityId>(8000));
    REQUIRE_FALSE(opt2.has_value());
}


