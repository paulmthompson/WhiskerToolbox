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

TEST_CASE("StateEstimation - MinCostFlowTracker - sliceMetaNodesToSegment trims single meta-node spanning anchors", "[MinCostFlowTracker][slice]") {
    // Meta node spans beyond anchors on both sides
    // frames 10..20 with unique ids 1000..1010
    std::vector<MetaNode> meta_nodes;
    std::vector<std::pair<long long, EntityId>> fe;
    for (long long f = 10; f <= 20; ++f) fe.push_back({f, static_cast<EntityId>(1000 + (f - 10))});
    meta_nodes.push_back(makeMetaNode(fe));

    // anchors at 12 and 18
    GroundTruthSegment seg;
    seg.group_id = static_cast<GroupId>(1);
    seg.start_frame = TimeFrameIndex(12);
    seg.start_entity = static_cast<EntityId>(1002);
    seg.end_frame = TimeFrameIndex(18);
    seg.end_entity = static_cast<EntityId>(1008);

    auto trimmed = sliceMetaNodesToSegment(meta_nodes, seg);
    REQUIRE(trimmed.size() == 1);
    REQUIRE(trimmed.front().members.front().frame == TimeFrameIndex(12));
    REQUIRE(trimmed.front().members.back().frame == TimeFrameIndex(18));
    REQUIRE(trimmed.front().members.size() == static_cast<size_t>((18 - 12) + 1));
}

TEST_CASE("StateEstimation - MinCostFlowTracker - sliceMetaNodesToSegment trims start/end and keeps interior", "[MinCostFlowTracker][slice]") {
    // Start meta spans before start anchor, end meta spans after end anchor
    // Interior node lies strictly within segment and should be kept entirely
    std::vector<MetaNode> meta_nodes;
    meta_nodes.push_back(makeMetaNode({{5, 1000}, {6, 1001}, {7, 1002}, {8, 1003}}));      // start meta
    meta_nodes.push_back(makeMetaNode({{9, 2000}, {10, 2001}}));                             // interior
    meta_nodes.push_back(makeMetaNode({{11, 3000}, {12, 3001}, {13, 3002}}));                // end meta

    GroundTruthSegment seg;
    seg.group_id = static_cast<GroupId>(1);
    seg.start_frame = TimeFrameIndex(7);
    seg.start_entity = static_cast<EntityId>(1002);
    seg.end_frame = TimeFrameIndex(12);
    seg.end_entity = static_cast<EntityId>(3001);

    auto trimmed = sliceMetaNodesToSegment(meta_nodes, seg);
    // Expect 3 nodes: trimmed start [7,8], interior [9,10], trimmed end [11,12]
    REQUIRE(trimmed.size() == 3);

    // start trimmed
    REQUIRE(trimmed[0].members.front().frame == TimeFrameIndex(7));
    REQUIRE(trimmed[0].members.back().frame == TimeFrameIndex(8));

    // interior unchanged
    REQUIRE(trimmed[1].members.front().frame == TimeFrameIndex(9));
    REQUIRE(trimmed[1].members.back().frame == TimeFrameIndex(10));

    // end trimmed
    REQUIRE(trimmed[2].members.front().frame == TimeFrameIndex(11));
    REQUIRE(trimmed[2].members.back().frame == TimeFrameIndex(12));
}

TEST_CASE("StateEstimation - MinCostFlowTracker - sliceMetaNodesToSegment excludes crossing nodes without anchors", "[MinCostFlowTracker][slice]") {
    // Build nodes such that one crosses a boundary but does not contain anchors
    std::vector<MetaNode> meta_nodes;
    meta_nodes.push_back(makeMetaNode({{5, 1000}, {6, 1001}, {7, 1002}}));                  // start meta (anchor at 7)
    meta_nodes.push_back(makeMetaNode({{8, 2100}, {9, 2101}, {10, 2102}}));                 // interior
    meta_nodes.push_back(makeMetaNode({{11, 2200}, {12, 2201}}));                           // interior
    meta_nodes.push_back(makeMetaNode({{12, 2300}, {13, 2301}}));                           // crosses end boundary but will not be included
    meta_nodes.push_back(makeMetaNode({{12, 3000}, {13, 3001}, {14, 3002}}));               // end meta (anchor at 13)

    GroundTruthSegment seg;
    seg.group_id = static_cast<GroupId>(2);
    seg.start_frame = TimeFrameIndex(7);
    seg.start_entity = static_cast<EntityId>(1002);
    seg.end_frame = TimeFrameIndex(13);
    seg.end_entity = static_cast<EntityId>(3001);

    auto trimmed = sliceMetaNodesToSegment(meta_nodes, seg);
    // Expected nodes: trimmed start [7], interior [8,9,10], interior [11,12], trimmed end [13]
    // The crossing node starting at 12 but not containing the end anchor should be excluded
    REQUIRE(trimmed.size() == 4);
    REQUIRE(trimmed[0].members.front().frame == TimeFrameIndex(7));
    REQUIRE(trimmed[0].members.back().frame == TimeFrameIndex(7));
    REQUIRE(trimmed[1].members.front().frame == TimeFrameIndex(8));
    REQUIRE(trimmed[1].members.back().frame == TimeFrameIndex(10));
    REQUIRE(trimmed[2].members.front().frame == TimeFrameIndex(11));
    REQUIRE(trimmed[2].members.back().frame == TimeFrameIndex(12));
    REQUIRE(trimmed[3].members.front().frame == TimeFrameIndex(12));
    REQUIRE(trimmed[3].members.back().frame == TimeFrameIndex(13));
}

TEST_CASE("StateEstimation - MinCostFlowTracker - sliceMetaNodesToSegment with multiple long nodes only one anchored", "[MinCostFlowTracker][slice]") {
    // Build start/end anchored nodes, multiple long nodes spanning beyond the range (no anchors), and interiors
    std::vector<MetaNode> meta_nodes;

    // Long node L1 spanning before start and after end, but no anchors of interest
    {
        std::vector<std::pair<long long, EntityId>> fe;
        for (long long f = 7; f <= 16; ++f) fe.push_back({f, static_cast<EntityId>(9000 + (f - 7))});
        meta_nodes.push_back(makeMetaNode(fe));
    }

    // Start meta node S with the start anchor at frame 10
    meta_nodes.push_back(makeMetaNode({{8, 1100}, {9, 1101}, {10, 1102}, {11, 1103}}));

    // Interior nodes entirely within (10,16)
    meta_nodes.push_back(makeMetaNode({{12, 2100}, {13, 2101}}));
    meta_nodes.push_back(makeMetaNode({{14, 2200}}));

    // Another long node L2 spanning the segment and beyond, no anchors
    {
        std::vector<std::pair<long long, EntityId>> fe;
        for (long long f = 9; f <= 18; ++f) fe.push_back({f, static_cast<EntityId>(9100 + (f - 9))});
        meta_nodes.push_back(makeMetaNode(fe));
    }

    // End meta node E with the end anchor at frame 16
    meta_nodes.push_back(makeMetaNode({{15, 3100}, {16, 3101}, {17, 3102}}));

    GroundTruthSegment seg;
    seg.group_id = static_cast<GroupId>(3);
    seg.start_frame = TimeFrameIndex(10);
    seg.start_entity = static_cast<EntityId>(1102);
    seg.end_frame = TimeFrameIndex(16);
    seg.end_entity = static_cast<EntityId>(3101);

    auto trimmed = sliceMetaNodesToSegment(meta_nodes, seg);

    // Expect: trimmed start [10,11], interior [12,13], interior [14], trimmed end [16]
    // Long nodes L1 and L2 should be excluded
    REQUIRE(trimmed.size() == 4);

    // start trimmed
    REQUIRE(trimmed[0].members.front().frame == TimeFrameIndex(10));
    REQUIRE(trimmed[0].members.back().frame == TimeFrameIndex(11));

    // interior 1
    REQUIRE(trimmed[1].members.front().frame == TimeFrameIndex(12));
    REQUIRE(trimmed[1].members.back().frame == TimeFrameIndex(13));

    // interior 2
    REQUIRE(trimmed[2].members.front().frame == TimeFrameIndex(14));
    REQUIRE(trimmed[2].members.back().frame == TimeFrameIndex(14));

    // end trimmed
    REQUIRE(trimmed[3].members.front().frame == TimeFrameIndex(15));
    REQUIRE(trimmed[3].members.back().frame == TimeFrameIndex(16));
}

TEST_CASE("StateEstimation - MinCostFlowTracker - anchors on a long spanning node are sliced, others excluded", "[MinCostFlowTracker][slice]") {
    // Use same shape of nodes as previous test but place both anchors on long node L2
    std::vector<MetaNode> meta_nodes;

    // Long node L1 spanning before start and after end, but without anchors
    {
        std::vector<std::pair<long long, EntityId>> fe;
        for (long long f = 7; f <= 16; ++f) fe.push_back({f, static_cast<EntityId>(9000 + (f - 7))});
        meta_nodes.push_back(makeMetaNode(fe));
    }

    // Start meta node S (will not be used as the anchor in this test)
    meta_nodes.push_back(makeMetaNode({{8, 1100}, {9, 1101}, {10, 1102}, {11, 1103}}));

    // Interior nodes entirely within (10,16)
    meta_nodes.push_back(makeMetaNode({{12, 2100}, {13, 2101}}));
    meta_nodes.push_back(makeMetaNode({{14, 2200}}));

    // Long node L2 spanning the segment and beyond; this one contains both anchors
    {
        std::vector<std::pair<long long, EntityId>> fe;
        for (long long f = 9; f <= 18; ++f) fe.push_back({f, static_cast<EntityId>(9100 + (f - 9))});
        meta_nodes.push_back(makeMetaNode(fe));
    }

    // End meta node E (not used as anchor)
    meta_nodes.push_back(makeMetaNode({{15, 3100}, {16, 3101}, {17, 3102}}));

    // Anchors are on L2 at frames 10 and 16
    GroundTruthSegment seg;
    seg.group_id = static_cast<GroupId>(4);
    seg.start_frame = TimeFrameIndex(10);
    seg.start_entity = static_cast<EntityId>(9101); // 9100 + (10-9)
    seg.end_frame = TimeFrameIndex(16);
    seg.end_entity = static_cast<EntityId>(9107);   // 9100 + (16-9)

    auto trimmed = sliceMetaNodesToSegment(meta_nodes, seg);

    // Because both anchors lie within the same meta-node, expect a single trimmed node [10..16]
    REQUIRE(trimmed.size() == 1);
    REQUIRE(trimmed.front().members.front().frame == TimeFrameIndex(10));
    REQUIRE(trimmed.front().members.back().frame == TimeFrameIndex(16));
    REQUIRE(trimmed.front().members.size() == static_cast<size_t>((16 - 10) + 1));
}

TEST_CASE("StateEstimation - MinCostFlowTracker - fallback path concatenates start and end trimmed nodes", "[MinCostFlowTracker][fallback]") {
    // Create trimmed meta-nodes for a segment where solver would fail; verify fallback concatenation
    std::vector<MetaNode> meta_nodes;
    meta_nodes.push_back(makeMetaNode({{10, 1100}, {11, 1101}, {12, 1102}})); // start node (anchor at 11)
    meta_nodes.push_back(makeMetaNode({{12, 2100}, {13, 2101}}));              // interior
    meta_nodes.push_back(makeMetaNode({{14, 3100}, {15, 3101}}));              // end node (anchor at 15)

    GroundTruthSegment seg;
    seg.group_id = static_cast<GroupId>(5);
    seg.start_frame = TimeFrameIndex(11);
    seg.start_entity = static_cast<EntityId>(1101);
    seg.end_frame = TimeFrameIndex(15);
    seg.end_entity = static_cast<EntityId>(3101);

    // Trim to segment
    auto trimmed = sliceMetaNodesToSegment(meta_nodes, seg);
    REQUIRE(trimmed.size() == 3);

    // Locate indices of start/end nodes in trimmed set
    auto pos_opt = findAnchorPositions(trimmed, seg);
    REQUIRE(pos_opt.has_value());
    int start_meta_index = pos_opt->start_meta_index;
    int end_meta_index = pos_opt->end_meta_index;

    auto fallback = buildFallbackPathFromTrimmed(trimmed, start_meta_index, end_meta_index);

    // Expect start members from [11,12] and end member [15]; interior [12,13] is not included by fallback concat
    REQUIRE_FALSE(fallback.empty());
    REQUIRE(fallback.front().frame == TimeFrameIndex(11));
    REQUIRE(fallback[1].frame == TimeFrameIndex(12));
    REQUIRE(fallback.back().frame == TimeFrameIndex(15));
}

TEST_CASE("StateEstimation - MinCostFlowTracker - fallback across consecutive segments retains boundary anchors and frame 99 entity", "[MinCostFlowTracker][fallback]") {
    // Build meta-nodes spanning two segments: [1->100] and [100->200]
    std::vector<MetaNode> meta_nodes;

    // Start node for segment 1: many frames up to 99 with unique entity ids; ensures frame 99 is present
    {
        std::vector<std::pair<long long, EntityId>> fe;
        for (long long f = 1; f <= 99; ++f) fe.push_back({f, static_cast<EntityId>(100 + f)}); // eid = 100+f; frame 99 -> eid 199
        meta_nodes.push_back(makeMetaNode(fe));
    }

    // Interior nodes within (1,100)
    meta_nodes.push_back(makeMetaNode({{50, 1500}, {51, 1501}}));

    // Boundary node at 100 which is also start anchor for segment 2
    meta_nodes.push_back(makeMetaNode({{100, 20100}, {101, 20101}}));

    // Interior nodes for segment 2
    meta_nodes.push_back(makeMetaNode({{150, 25100}}));
    meta_nodes.push_back(makeMetaNode({{180, 28100}}));

    // End node for segment 2 at 200
    meta_nodes.push_back(makeMetaNode({{200, 20200}, {201, 20201}}));

    // Segment 1 anchors
    GroundTruthSegment seg1;
    seg1.group_id = static_cast<GroupId>(6);
    seg1.start_frame = TimeFrameIndex(1);
    seg1.start_entity = static_cast<EntityId>(101);
    seg1.end_frame = TimeFrameIndex(100);
    seg1.end_entity = static_cast<EntityId>(20100);

    // Segment 2 anchors
    GroundTruthSegment seg2;
    seg2.group_id = static_cast<GroupId>(6);
    seg2.start_frame = TimeFrameIndex(100);
    seg2.start_entity = static_cast<EntityId>(20100);
    seg2.end_frame = TimeFrameIndex(200);
    seg2.end_entity = static_cast<EntityId>(20200);

    // Trim both segments
    auto trimmed1 = sliceMetaNodesToSegment(meta_nodes, seg1);
    auto trimmed2 = sliceMetaNodesToSegment(meta_nodes, seg2);
    REQUIRE_FALSE(trimmed1.empty());
    REQUIRE_FALSE(trimmed2.empty());

    // Indices for fallback
    auto pos1 = findAnchorPositions(trimmed1, seg1);
    auto pos2 = findAnchorPositions(trimmed2, seg2);
    REQUIRE(pos1.has_value());
    REQUIRE(pos2.has_value());

    auto fb1 = buildFallbackPathFromTrimmed(trimmed1, pos1->start_meta_index, pos1->end_meta_index);
    auto fb2 = buildFallbackPathFromTrimmed(trimmed2, pos2->start_meta_index, pos2->end_meta_index);

    // Mimic deduplication in solve_flow_over_segments
    if (!fb1.empty() && !fb2.empty()) {
        if (fb1.back().frame == fb2.front().frame && fb1.back().entity_id == fb2.front().entity_id) {
            fb2.erase(fb2.begin());
        }
    }

    Path combined;
    combined.insert(combined.end(), fb1.begin(), fb1.end());
    combined.insert(combined.end(), fb2.begin(), fb2.end());

    // Expect that frame 100 is present exactly once and frame 200 is present
    int count100 = 0;
    for (auto const & n : combined) if (n.frame == TimeFrameIndex(100)) ++count100;
    REQUIRE(count100 == 1);
    REQUIRE(combined.back().frame == TimeFrameIndex(200));

    // Specifically ensure frame 99 is present with the expected entity id from the spliced start node
    bool found99 = false;
    EntityId eid99 = 0;
    for (auto const & n : fb1) {
        if (n.frame == TimeFrameIndex(99)) {
            found99 = true;
            eid99 = n.entity_id;
            break;
        }
    }
    REQUIRE(found99);
    REQUIRE(eid99 == static_cast<EntityId>(199));
}

TEST_CASE("StateEstimation - MinCostFlowTracker - overlapping long tracklets with anchors at 1,100,200 include all entities in fallback", "[MinCostFlowTracker][fallback]") {
    // Node A: frames 1..150 with unique eids (baseA + f)
    // Node B: frames 51..200 with unique eids (baseB + f)
    // Anchors at 1 (A), 100 (present in A and B), and 200 (B)
    std::vector<MetaNode> meta_nodes;

    {
        std::vector<std::pair<long long, EntityId>> fe;
        for (long long f = 1; f <= 150; ++f) fe.push_back({f, static_cast<EntityId>(10000 + f)});
        meta_nodes.push_back(makeMetaNode(fe));
    }
    {
        std::vector<std::pair<long long, EntityId>> fe;
        for (long long f = 180; f <= 200; ++f) fe.push_back({f, static_cast<EntityId>(20000 + f)});
        meta_nodes.push_back(makeMetaNode(fe));
    }

    GroundTruthSegment seg1;
    seg1.group_id = static_cast<GroupId>(7);
    seg1.start_frame = TimeFrameIndex(1);
    seg1.start_entity = static_cast<EntityId>(10001);
    seg1.end_frame = TimeFrameIndex(100);
    seg1.end_entity = static_cast<EntityId>(10100); // from node A

    GroundTruthSegment seg2;
    seg2.group_id = static_cast<GroupId>(7);
    seg2.start_frame = TimeFrameIndex(100);
    seg2.start_entity = static_cast<EntityId>(10100); // from node A
    seg2.end_frame = TimeFrameIndex(200);
    seg2.end_entity = static_cast<EntityId>(20200);

    auto trimmed1 = sliceMetaNodesToSegment(meta_nodes, seg1);
    auto trimmed2 = sliceMetaNodesToSegment(meta_nodes, seg2);
    REQUIRE_FALSE(trimmed1.empty());
    REQUIRE_FALSE(trimmed2.empty());

    auto pos1 = findAnchorPositions(trimmed1, seg1);
    auto pos2 = findAnchorPositions(trimmed2, seg2);
    REQUIRE(pos1.has_value());
    REQUIRE(pos2.has_value());

    auto fb1 = buildFallbackPathFromTrimmed(trimmed1, pos1->start_meta_index, pos1->end_meta_index);
    auto fb2 = buildFallbackPathFromTrimmed(trimmed2, pos2->start_meta_index, pos2->end_meta_index);

    // Dedup boundary (frame 100) like production path concatenation
    if (!fb1.empty() && !fb2.empty()) {
        if (fb1.back().frame == fb2.front().frame && fb1.back().entity_id == fb2.front().entity_id) {
            fb2.erase(fb2.begin());
        }
    }

    Path combined;
    combined.insert(combined.end(), fb1.begin(), fb1.end());
    combined.insert(combined.end(), fb2.begin(), fb2.end());

    // Verify coverage: frames 1..200 present; 1..99 from A, 100..200 from B
    // Check a few representative frames
    auto find_entity = [&](int frame) -> std::optional<EntityId> {
        for (auto const & n : combined) if (n.frame == TimeFrameIndex(frame)) return n.entity_id;
        return std::nullopt;
    };

    REQUIRE(find_entity(1).has_value());
    REQUIRE(find_entity(99).has_value());
    REQUIRE(find_entity(100).has_value());
    REQUIRE(find_entity(150).has_value());
    REQUIRE(find_entity(180).has_value());
    REQUIRE(find_entity(200).has_value());

    REQUIRE(find_entity(1).value() == static_cast<EntityId>(10001));
    REQUIRE(find_entity(99).value() == static_cast<EntityId>(10099));
    REQUIRE(find_entity(100).value() == static_cast<EntityId>(10100));
    REQUIRE(find_entity(150).value() == static_cast<EntityId>(10150)); 
    REQUIRE(find_entity(180).value() == static_cast<EntityId>(20180));
    REQUIRE(find_entity(200).value() == static_cast<EntityId>(20200));
}
