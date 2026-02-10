#include <catch2/catch_test_macros.hpp>

#include "models_v2/ModelBase.hpp"
#include "models_v2/TensorSlotDescriptor.hpp"

#include <torch/torch.h>

namespace {

/// A minimal concrete implementation of ModelBase for testing purposes.
class DummyModel : public dl::ModelBase {
public:
    std::string modelId() const override { return "dummy"; }
    std::string displayName() const override { return "Dummy Model"; }
    std::string description() const override { return "A test-only model"; }

    std::vector<dl::TensorSlotDescriptor> inputSlots() const override {
        return {
            {.name = "image", .shape = {3, 64, 64}, .description = "Input image",
             .recommended_encoder = "ImageEncoder"},
            {.name = "mask", .shape = {1, 64, 64}, .description = "Input mask",
             .recommended_encoder = "Mask2DEncoder", .is_static = true},
        };
    }

    std::vector<dl::TensorSlotDescriptor> outputSlots() const override {
        return {
            {.name = "heatmap", .shape = {1, 64, 64}, .description = "Output heatmap",
             .recommended_decoder = "TensorToMask2D"},
        };
    }

    void loadWeights(std::filesystem::path const & /*path*/) override {
        _ready = true;
    }

    bool isReady() const override { return _ready; }

    int preferredBatchSize() const override { return 1; }
    int maxBatchSize() const override { return 8; }

    std::unordered_map<std::string, torch::Tensor>
    forward(std::unordered_map<std::string, torch::Tensor> const & inputs) override {
        // Simple passthrough: output = sigmoid(mean of inputs across channels)
        auto it = inputs.find("image");
        if (it == inputs.end()) {
            return {};
        }
        auto input = it->second;
        // Mean across channel dim → [B, 1, H, W]
        auto output = input.mean(/*dim=*/1, /*keepdim=*/true).sigmoid();
        return {{"heatmap", output}};
    }

private:
    bool _ready = false;
};

/// Another dummy model with default batch size methods.
class MinimalModel : public dl::ModelBase {
public:
    std::string modelId() const override { return "minimal"; }
    std::string displayName() const override { return "Minimal"; }
    std::string description() const override { return "Minimal test model"; }

    std::vector<dl::TensorSlotDescriptor> inputSlots() const override { return {}; }
    std::vector<dl::TensorSlotDescriptor> outputSlots() const override { return {}; }

    void loadWeights(std::filesystem::path const & /*path*/) override {}
    bool isReady() const override { return false; }

    std::unordered_map<std::string, torch::Tensor>
    forward(std::unordered_map<std::string, torch::Tensor> const & /*inputs*/) override {
        return {};
    }
};

} // anonymous namespace

TEST_CASE("ModelBase - DummyModel metadata", "[ModelBase]")
{
    DummyModel model;

    CHECK(model.modelId() == "dummy");
    CHECK(model.displayName() == "Dummy Model");
    CHECK(model.description() == "A test-only model");
}

TEST_CASE("ModelBase - input/output slot descriptors", "[ModelBase]")
{
    DummyModel model;

    auto inputs = model.inputSlots();
    REQUIRE(inputs.size() == 2);
    CHECK(inputs[0].name == "image");
    auto const expected_image_shape = std::vector<int64_t>{3, 64, 64};
    CHECK(inputs[0].shape == expected_image_shape);
    CHECK(inputs[0].recommended_encoder == "ImageEncoder");
    CHECK(inputs[0].is_static == false);

    CHECK(inputs[1].name == "mask");
    CHECK(inputs[1].is_static == true);

    auto outputs = model.outputSlots();
    REQUIRE(outputs.size() == 1);
    CHECK(outputs[0].name == "heatmap");
    CHECK(outputs[0].recommended_decoder == "TensorToMask2D");
}

TEST_CASE("ModelBase - weight loading and readiness", "[ModelBase]")
{
    DummyModel model;

    CHECK(model.isReady() == false);
    model.loadWeights("/fake/path.pte");
    CHECK(model.isReady() == true);
}

TEST_CASE("ModelBase - batch size defaults", "[ModelBase]")
{
    SECTION("DummyModel with custom batch sizes") {
        DummyModel model;
        CHECK(model.preferredBatchSize() == 1);
        CHECK(model.maxBatchSize() == 8);
    }

    SECTION("MinimalModel with default batch sizes") {
        MinimalModel model;
        CHECK(model.preferredBatchSize() == 0);
        CHECK(model.maxBatchSize() == 0);
    }
}

TEST_CASE("ModelBase - forward pass with DummyModel", "[ModelBase]")
{
    DummyModel model;
    model.loadWeights("/fake/path.pte");

    auto image = torch::randn({1, 3, 64, 64});
    auto mask = torch::ones({1, 1, 64, 64});

    std::unordered_map<std::string, torch::Tensor> inputs{
        {"image", image},
        {"mask", mask}
    };

    auto outputs = model.forward(inputs);

    REQUIRE(outputs.count("heatmap") == 1);
    auto heatmap = outputs.at("heatmap");
    CHECK(heatmap.dim() == 4);
    CHECK(heatmap.size(0) == 1);  // batch
    CHECK(heatmap.size(1) == 1);  // channels
    CHECK(heatmap.size(2) == 64); // height
    CHECK(heatmap.size(3) == 64); // width

    // Sigmoid output should be in [0, 1]
    CHECK(heatmap.min().item<float>() >= 0.0f);
    CHECK(heatmap.max().item<float>() <= 1.0f);
}

TEST_CASE("ModelBase - forward with batch > 1", "[ModelBase]")
{
    DummyModel model;
    model.loadWeights("/fake/path.pte");

    int const batch_size = 4;
    auto image = torch::randn({batch_size, 3, 64, 64});

    auto outputs = model.forward({{"image", image}});

    REQUIRE(outputs.count("heatmap") == 1);
    CHECK(outputs.at("heatmap").size(0) == batch_size);
}

TEST_CASE("ModelBase - forward with missing input returns empty", "[ModelBase]")
{
    DummyModel model;
    model.loadWeights("/fake/path.pte");

    // No "image" key → DummyModel returns empty
    auto outputs = model.forward({{"wrong_key", torch::randn({1, 3, 64, 64})}});
    CHECK(outputs.empty());
}
