#include <catch2/catch_test_macros.hpp>

#include "models_v2/ModelBase.hpp"
#include "models_v2/TensorSlotDescriptor.hpp"
#include "registry/ModelRegistry.hpp"

#include <torch/torch.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace {

/// A simple test model for registry tests.
class AlphaModel : public dl::ModelBase {
public:
    std::string modelId() const override { return "alpha"; }
    std::string displayName() const override { return "Alpha Model"; }
    std::string description() const override { return "First test model"; }

    std::vector<dl::TensorSlotDescriptor> inputSlots() const override {
        return {
            {.name = "image", .shape = {3, 128, 128}, .description = "Input image",
             .recommended_encoder = "ImageEncoder"},
        };
    }

    std::vector<dl::TensorSlotDescriptor> outputSlots() const override {
        return {
            {.name = "heatmap", .shape = {1, 128, 128}, .description = "Output heatmap",
             .recommended_decoder = "TensorToMask2D"},
        };
    }

    void loadWeights(std::filesystem::path const & /*path*/) override { _ready = true; }
    bool isReady() const override { return _ready; }

    int preferredBatchSize() const override { return 1; }
    int maxBatchSize() const override { return 16; }

    std::unordered_map<std::string, torch::Tensor>
    forward(std::unordered_map<std::string, torch::Tensor> const & inputs) override {
        auto it = inputs.find("image");
        if (it == inputs.end()) { return {}; }
        auto output = it->second.mean(/*dim=*/1, /*keepdim=*/true).sigmoid();
        return {{"heatmap", output}};
    }

private:
    bool _ready = false;
};

/// Another test model with different metadata.
class BetaModel : public dl::ModelBase {
public:
    std::string modelId() const override { return "beta"; }
    std::string displayName() const override { return "Beta Model"; }
    std::string description() const override { return "Second test model"; }

    std::vector<dl::TensorSlotDescriptor> inputSlots() const override {
        return {
            {.name = "frame", .shape = {3, 64, 64}, .description = "Video frame",
             .recommended_encoder = "ImageEncoder"},
            {.name = "mask", .shape = {1, 64, 64}, .description = "ROI mask",
             .recommended_encoder = "Mask2DEncoder", .is_static = true},
        };
    }

    std::vector<dl::TensorSlotDescriptor> outputSlots() const override {
        return {
            {.name = "points", .shape = {2}, .description = "Detected point",
             .recommended_decoder = "TensorToPoint2D"},
            {.name = "confidence", .shape = {1}, .description = "Confidence score"},
        };
    }

    void loadWeights(std::filesystem::path const & /*path*/) override {}
    bool isReady() const override { return false; }

    std::unordered_map<std::string, torch::Tensor>
    forward(std::unordered_map<std::string, torch::Tensor> const & /*inputs*/) override {
        return {};
    }
};

/// Helper that registers models fresh for each test, then removes them on scope exit.
/// Does NOT call clear() to avoid destroying static-init registered models (e.g. from the macro).
struct RegistryFixture {
    RegistryFixture() {
        auto & reg = dl::ModelRegistry::instance();
        reg.unregisterModel("alpha");
        reg.unregisterModel("beta");
        reg.registerModel("alpha", [] { return std::make_unique<AlphaModel>(); });
        reg.registerModel("beta", [] { return std::make_unique<BetaModel>(); });
    }

    ~RegistryFixture() {
        auto & reg = dl::ModelRegistry::instance();
        reg.unregisterModel("alpha");
        reg.unregisterModel("beta");
    }
};

} // anonymous namespace

// ─── Singleton ───────────────────────────────────────────────────

TEST_CASE("ModelRegistry - singleton identity", "[ModelRegistry]")
{
    auto & a = dl::ModelRegistry::instance();
    auto & b = dl::ModelRegistry::instance();
    CHECK(&a == &b);
}

// ─── Registration & Enumeration ──────────────────────────────────

TEST_CASE("ModelRegistry - register and enumerate models", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto const & reg = dl::ModelRegistry::instance();

    auto ids = reg.availableModels();
    REQUIRE(ids.size() >= 2);
    // Both alpha and beta should be in the sorted list
    CHECK(std::find(ids.begin(), ids.end(), "alpha") != ids.end());
    CHECK(std::find(ids.begin(), ids.end(), "beta") != ids.end());
    // Verify sorted order
    CHECK(std::is_sorted(ids.begin(), ids.end()));
}

TEST_CASE("ModelRegistry - size reports correct count", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto const & reg = dl::ModelRegistry::instance();

    // At least alpha and beta; gamma may also be present from macro registration
    CHECK(reg.size() >= 2);
    CHECK(reg.hasModel("alpha"));
    CHECK(reg.hasModel("beta"));
}

TEST_CASE("ModelRegistry - hasModel", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto const & reg = dl::ModelRegistry::instance();

    CHECK(reg.hasModel("alpha"));
    CHECK(reg.hasModel("beta"));
    CHECK_FALSE(reg.hasModel("nonexistent_xyz_model"));
    CHECK_FALSE(reg.hasModel(""));
}

TEST_CASE("ModelRegistry - unregister empties fixture models", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto & reg = dl::ModelRegistry::instance();

    reg.unregisterModel("alpha");
    reg.unregisterModel("beta");
    CHECK_FALSE(reg.hasModel("alpha"));
    CHECK_FALSE(reg.hasModel("beta"));
}

// ─── Creation ────────────────────────────────────────────────────

TEST_CASE("ModelRegistry - create returns correct model", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto const & reg = dl::ModelRegistry::instance();

    auto alpha = reg.create("alpha");
    REQUIRE(alpha != nullptr);
    CHECK(alpha->modelId() == "alpha");
    CHECK(alpha->displayName() == "Alpha Model");

    auto beta = reg.create("beta");
    REQUIRE(beta != nullptr);
    CHECK(beta->modelId() == "beta");
    CHECK(beta->displayName() == "Beta Model");
}

TEST_CASE("ModelRegistry - create unknown returns nullptr", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto const & reg = dl::ModelRegistry::instance();

    auto model = reg.create("nonexistent");
    CHECK(model == nullptr);
}

TEST_CASE("ModelRegistry - create returns independent instances", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto const & reg = dl::ModelRegistry::instance();

    auto a1 = reg.create("alpha");
    auto a2 = reg.create("alpha");
    REQUIRE(a1 != nullptr);
    REQUIRE(a2 != nullptr);
    CHECK(a1.get() != a2.get());

    // Modifying one shouldn't affect the other
    a1->loadWeights("/fake/path.pte");
    CHECK(a1->isReady());
    CHECK_FALSE(a2->isReady());
}

// ─── ModelInfo ───────────────────────────────────────────────────

TEST_CASE("ModelRegistry - getModelInfo for AlphaModel", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto const & reg = dl::ModelRegistry::instance();

    auto info = reg.getModelInfo("alpha");
    REQUIRE(info.has_value());

    CHECK(info->model_id == "alpha");
    CHECK(info->display_name == "Alpha Model");
    CHECK(info->description == "First test model");
    CHECK(info->preferred_batch_size == 1);
    CHECK(info->max_batch_size == 16);

    REQUIRE(info->inputs.size() == 1);
    CHECK(info->inputs[0].name == "image");
    auto const expected_shape = std::vector<int64_t>{3, 128, 128};
    CHECK(info->inputs[0].shape == expected_shape);
    CHECK(info->inputs[0].recommended_encoder == "ImageEncoder");

    REQUIRE(info->outputs.size() == 1);
    CHECK(info->outputs[0].name == "heatmap");
    CHECK(info->outputs[0].recommended_decoder == "TensorToMask2D");
}

TEST_CASE("ModelRegistry - getModelInfo for BetaModel", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto const & reg = dl::ModelRegistry::instance();

    auto info = reg.getModelInfo("beta");
    REQUIRE(info.has_value());

    CHECK(info->model_id == "beta");
    REQUIRE(info->inputs.size() == 2);
    CHECK(info->inputs[0].name == "frame");
    CHECK(info->inputs[1].name == "mask");
    CHECK(info->inputs[1].is_static);

    REQUIRE(info->outputs.size() == 2);
    CHECK(info->outputs[0].name == "points");
    CHECK(info->outputs[1].name == "confidence");
}

TEST_CASE("ModelRegistry - getModelInfo unknown returns nullopt", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto const & reg = dl::ModelRegistry::instance();

    auto info = reg.getModelInfo("nonexistent");
    CHECK_FALSE(info.has_value());
}

TEST_CASE("ModelRegistry - getModelInfo caches results", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto const & reg = dl::ModelRegistry::instance();

    // First call populates cache
    auto info1 = reg.getModelInfo("alpha");
    // Second call should return the cached version
    auto info2 = reg.getModelInfo("alpha");

    REQUIRE(info1.has_value());
    REQUIRE(info2.has_value());
    CHECK(info1->model_id == info2->model_id);
    CHECK(info1->display_name == info2->display_name);
    CHECK(info1->inputs.size() == info2->inputs.size());
}

// ─── Slot Lookup ─────────────────────────────────────────────────

TEST_CASE("ModelRegistry - getInputSlot finds existing slot", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto const & reg = dl::ModelRegistry::instance();

    auto const * slot = reg.getInputSlot("alpha", "image");
    REQUIRE(slot != nullptr);
    CHECK(slot->name == "image");
    CHECK(slot->recommended_encoder == "ImageEncoder");
    auto const expected_shape = std::vector<int64_t>{3, 128, 128};
    CHECK(slot->shape == expected_shape);
}

TEST_CASE("ModelRegistry - getInputSlot returns nullptr for unknown slot", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto const & reg = dl::ModelRegistry::instance();

    CHECK(reg.getInputSlot("alpha", "nonexistent") == nullptr);
}

TEST_CASE("ModelRegistry - getInputSlot returns nullptr for unknown model", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto const & reg = dl::ModelRegistry::instance();

    CHECK(reg.getInputSlot("nonexistent", "image") == nullptr);
}

TEST_CASE("ModelRegistry - getOutputSlot finds existing slot", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto const & reg = dl::ModelRegistry::instance();

    auto const * slot = reg.getOutputSlot("alpha", "heatmap");
    REQUIRE(slot != nullptr);
    CHECK(slot->name == "heatmap");
    CHECK(slot->recommended_decoder == "TensorToMask2D");
}

TEST_CASE("ModelRegistry - getOutputSlot returns nullptr for unknown slot", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto const & reg = dl::ModelRegistry::instance();

    CHECK(reg.getOutputSlot("beta", "nonexistent") == nullptr);
}

// ─── Unregister ──────────────────────────────────────────────────

TEST_CASE("ModelRegistry - unregister removes model", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto & reg = dl::ModelRegistry::instance();

    auto const size_before = reg.size();
    CHECK(reg.hasModel("alpha"));
    CHECK(reg.unregisterModel("alpha"));
    CHECK_FALSE(reg.hasModel("alpha"));
    CHECK(reg.size() == size_before - 1);
    CHECK(reg.create("alpha") == nullptr);
    CHECK_FALSE(reg.getModelInfo("alpha").has_value());
}

TEST_CASE("ModelRegistry - unregister unknown returns false", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto & reg = dl::ModelRegistry::instance();

    auto const size_before = reg.size();
    CHECK_FALSE(reg.unregisterModel("nonexistent"));
    CHECK(reg.size() == size_before);
}

// ─── Re-registration (override) ─────────────────────────────────

TEST_CASE("ModelRegistry - re-registering overrides factory", "[ModelRegistry]")
{
    RegistryFixture fixture;
    auto & reg = dl::ModelRegistry::instance();

    // Override "alpha" with BetaModel factory
    reg.registerModel("alpha", [] { return std::make_unique<BetaModel>(); });

    CHECK(reg.hasModel("alpha")); // still present after overwrite

    auto model = reg.create("alpha");
    REQUIRE(model != nullptr);
    // The factory now returns a BetaModel, so modelId() returns "beta"
    CHECK(model->modelId() == "beta");

    // Info cache should be invalidated
    auto info = reg.getModelInfo("alpha");
    REQUIRE(info.has_value());
    CHECK(info->display_name == "Beta Model");
}

// ─── DL_REGISTER_MODEL Macro ────────────────────────────────────

// We create a model class here and register it with the macro to verify it works.
namespace {

class GammaModel : public dl::ModelBase {
public:
    std::string modelId() const override { return "gamma"; }
    std::string displayName() const override { return "Gamma Model"; }
    std::string description() const override { return "Macro-registered model"; }

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

// The macro must be at namespace scope (not inside a function or anonymous namespace
// that conflicts with the macro's own anonymous namespace). We place it here:
DL_REGISTER_MODEL(GammaModel)

TEST_CASE("ModelRegistry - DL_REGISTER_MODEL macro auto-registers", "[ModelRegistry]")
{
    // Note: we do NOT use RegistryFixture here because it clears the registry.
    // GammaModel was registered at static init time by the macro.
    auto const & reg = dl::ModelRegistry::instance();

    CHECK(reg.hasModel("gamma"));

    auto model = reg.create("gamma");
    REQUIRE(model != nullptr);
    CHECK(model->modelId() == "gamma");
    CHECK(model->displayName() == "Gamma Model");

    auto info = reg.getModelInfo("gamma");
    REQUIRE(info.has_value());
    CHECK(info->description == "Macro-registered model");

    // Clean up so this doesn't leak into other tests
    dl::ModelRegistry::instance().unregisterModel("gamma");
}
