#include "TensorDesign/TensorDesignBuilder.hpp"
#include "TensorDesign/DesignPresetRegistry.hpp"
#include "DataManager.hpp"
#include "Tensors/TensorData.hpp"
#include "DataObjects/Points/Point_Data.hpp"
#include "fixtures/whisker_contact_test_fixture.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <map>

using namespace Neuralyzer::TensorDesign;

int main() {
    std::cout << "Initializing scenario..." << std::endl;
    WhiskerContactScenarioConfig cfg;
    cfg.duration_sec = 60.0;
    WhiskerContactTestFixture fixture(cfg);
    
    DataManager& dm = fixture.dm();
    auto const& scenario = fixture.scenario();
    auto time_frame = scenario.time_time;

    int num_points = 50;
    std::vector<std::string> point_keys;
    
    std::cout << "Creating PointData..." << std::endl;
    for (int i = 0; i < num_points; ++i) {
        std::string p_key = "point_" + std::to_string(i);
        point_keys.push_back(p_key);
        
        std::map<TimeFrameIndex, Point2D<float>> pdata;
        for (int64_t t = 0; t < time_frame->getTotalFrameCount(); t+=10) {
            pdata[TimeFrameIndex(t)] = Point2D<float>{static_cast<float>(i), static_cast<float>(t)};
        }
        
        auto pt = std::make_shared<PointData>(pdata);
        pt->setTimeFrame(time_frame);
        dm.setData<PointData>(p_key, pt, TimeKey(cfg.time_time_key));
    }
    
    std::cout << "Creating Design Preset..." << std::endl;
    DesignPresetRegistry registry = createBuiltInDesignPresetRegistry();
    
    DesignPresetArgs args;
    args.row_source_key = cfg.contact_key;
    args.curvature_source_key = cfg.curvature_key;
    args.spike_source_key = cfg.spikes_key;
    args.angle_source_key = cfg.angle_key;
    args.onset_pre = 10;
    args.onset_post = 10;
    args.keypoint_source_keys = point_keys;
    
    auto expansion = registry.expand("whisker_contact_feature_table", args);
    if (!expansion) {
        std::cerr << "Failed to expand preset!" << std::endl;
        return 1;
    }
    
    std::cout << "Building tensor..." << std::endl;
    auto tensor = buildTensor(dm, expansion->spec);
    
    if (tensor) {
        std::cout << "Tensor built with " << tensor->numRows() << " rows and " << tensor->numColumns() << " columns." << std::endl;
        std::cout << "Materializing lazy columns..." << std::endl;
        auto materialized = tensor->materialize();
        std::cout << "Materialized!" << std::endl;
    } else {
        std::cerr << "Failed to build tensor!" << std::endl;
        return 1;
    }
    
    return 0;
}
