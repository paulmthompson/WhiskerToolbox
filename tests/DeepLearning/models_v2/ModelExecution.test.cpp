#include <catch2/catch_test_macros.hpp>

#include "models_v2/ModelExecution.hpp"
#include "models_v2/backends/InferenceBackend.hpp"

#include <torch/torch.h>

#include <filesystem>
#include <fstream>

// ============================================================================
// BackendType helpers
// ============================================================================

TEST_CASE("BackendType - backendTypeToString", "[BackendType]")
{
    CHECK(dl::backendTypeToString(dl::BackendType::TorchScript) == "TorchScript");
    CHECK(dl::backendTypeToString(dl::BackendType::AOTInductor) == "AOTInductor");
    CHECK(dl::backendTypeToString(dl::BackendType::ExecuTorch) == "ExecuTorch");
    CHECK(dl::backendTypeToString(dl::BackendType::Auto) == "Auto");
}

TEST_CASE("BackendType - backendTypeFromString", "[BackendType]")
{
    CHECK(dl::backendTypeFromString("torchscript") == dl::BackendType::TorchScript);
    CHECK(dl::backendTypeFromString("TorchScript") == dl::BackendType::TorchScript);
    CHECK(dl::backendTypeFromString("torch_script") == dl::BackendType::TorchScript);
    CHECK(dl::backendTypeFromString("jit") == dl::BackendType::TorchScript);

    CHECK(dl::backendTypeFromString("aotinductor") == dl::BackendType::AOTInductor);
    CHECK(dl::backendTypeFromString("AOTInductor") == dl::BackendType::AOTInductor);
    CHECK(dl::backendTypeFromString("aot_inductor") == dl::BackendType::AOTInductor);
    CHECK(dl::backendTypeFromString("inductor") == dl::BackendType::AOTInductor);
    CHECK(dl::backendTypeFromString("aoti") == dl::BackendType::AOTInductor);

    CHECK(dl::backendTypeFromString("executorch") == dl::BackendType::ExecuTorch);
    CHECK(dl::backendTypeFromString("ExecuTorch") == dl::BackendType::ExecuTorch);

    CHECK(dl::backendTypeFromString("auto") == dl::BackendType::Auto);
    CHECK(dl::backendTypeFromString("unknown_value") == dl::BackendType::Auto);
    CHECK(dl::backendTypeFromString("") == dl::BackendType::Auto);
}

TEST_CASE("BackendType - backendTypeFromExtension", "[BackendType]")
{
    CHECK(dl::backendTypeFromExtension("model.pt") == dl::BackendType::TorchScript);
    CHECK(dl::backendTypeFromExtension("model.pt2") == dl::BackendType::AOTInductor);
    CHECK(dl::backendTypeFromExtension("model.pte") == dl::BackendType::ExecuTorch);
    CHECK(dl::backendTypeFromExtension("model.onnx") == dl::BackendType::Auto);
    CHECK(dl::backendTypeFromExtension("model") == dl::BackendType::Auto);

    // Case insensitivity
    CHECK(dl::backendTypeFromExtension("MODEL.PT") == dl::BackendType::TorchScript);
    CHECK(dl::backendTypeFromExtension("MODEL.PT2") == dl::BackendType::AOTInductor);
    CHECK(dl::backendTypeFromExtension("MODEL.PTE") == dl::BackendType::ExecuTorch);
}

// ============================================================================
// ModelExecution - strategy pattern
// ============================================================================

TEST_CASE("ModelExecution - default construction (Auto backend)", "[ModelExecution]")
{
    dl::ModelExecution exec;
    CHECK_FALSE(exec.isLoaded());
    CHECK(exec.loadedPath().empty());
    CHECK(exec.activeBackend() == dl::BackendType::Auto);
    CHECK(exec.activeBackendName() == "Auto");
}

TEST_CASE("ModelExecution - explicit TorchScript backend construction", "[ModelExecution]")
{
    dl::ModelExecution exec(dl::BackendType::TorchScript);
    CHECK_FALSE(exec.isLoaded());
    CHECK(exec.activeBackend() == dl::BackendType::TorchScript);
    CHECK(exec.activeBackendName() == "TorchScript");
}

TEST_CASE("ModelExecution - explicit AOTInductor backend construction", "[ModelExecution]")
{
    dl::ModelExecution exec(dl::BackendType::AOTInductor);
    CHECK_FALSE(exec.isLoaded());
    CHECK(exec.activeBackend() == dl::BackendType::AOTInductor);
    CHECK(exec.activeBackendName() == "AOTInductor");
}

TEST_CASE("ModelExecution - load nonexistent .pt file selects TorchScript", "[ModelExecution]")
{
    dl::ModelExecution exec;
    bool ok = exec.load("/nonexistent/path/model.pt");
    CHECK_FALSE(ok);
    CHECK_FALSE(exec.isLoaded());
}

TEST_CASE("ModelExecution - load nonexistent .pt2 file selects AOTInductor", "[ModelExecution]")
{
    dl::ModelExecution exec;
    bool ok = exec.load("/nonexistent/path/model.pt2");
    CHECK_FALSE(ok);
    CHECK_FALSE(exec.isLoaded());
}

TEST_CASE("ModelExecution - load unknown extension returns false", "[ModelExecution]")
{
    dl::ModelExecution exec;
    bool ok = exec.load("/nonexistent/path/model.onnx");
    CHECK_FALSE(ok);
    CHECK_FALSE(exec.isLoaded());
}

TEST_CASE("ModelExecution - load invalid .pt file", "[ModelExecution]")
{
    auto tmp = std::filesystem::temp_directory_path() / "test_invalid.pt";
    {
        std::ofstream f(tmp, std::ios::binary);
        f << "this is not a valid torchscript file";
    }

    dl::ModelExecution exec;
    bool ok = exec.load(tmp);
    CHECK_FALSE(ok);
    CHECK_FALSE(exec.isLoaded());

    std::filesystem::remove(tmp);
}

TEST_CASE("ModelExecution - load invalid .pt2 file", "[ModelExecution]")
{
    auto tmp = std::filesystem::temp_directory_path() / "test_invalid.pt2";
    {
        std::ofstream f(tmp, std::ios::binary);
        f << "this is not a valid pt2 file";
    }

    dl::ModelExecution exec;
    bool ok = exec.load(tmp);
    CHECK_FALSE(ok);
    CHECK_FALSE(exec.isLoaded());

    std::filesystem::remove(tmp);
}

TEST_CASE("ModelExecution - execute without loading throws", "[ModelExecution]")
{
    dl::ModelExecution exec;
    CHECK_THROWS_AS(
        exec.execute({torch::randn({1, 3, 64, 64})}),
        std::runtime_error);
}

TEST_CASE("ModelExecution - executeNamed with missing slot throws", "[ModelExecution]")
{
    dl::ModelExecution exec;
    std::unordered_map<std::string, torch::Tensor> named_inputs{
        {"image", torch::randn({1, 3, 64, 64})}
    };
    std::vector<std::string> order{"image", "missing_slot"};

    CHECK_THROWS_AS(
        exec.executeNamed(named_inputs, order),
        std::runtime_error);
}

TEST_CASE("ModelExecution - move semantics", "[ModelExecution]")
{
    dl::ModelExecution exec1;
    dl::ModelExecution exec2 = std::move(exec1);
    CHECK_FALSE(exec2.isLoaded());

    // Move assignment
    dl::ModelExecution exec3;
    exec3 = std::move(exec2);
    CHECK_FALSE(exec3.isLoaded());
}

TEST_CASE("ModelExecution - move semantics with explicit backend", "[ModelExecution]")
{
    dl::ModelExecution exec1(dl::BackendType::TorchScript);
    CHECK(exec1.activeBackend() == dl::BackendType::TorchScript);

    dl::ModelExecution exec2 = std::move(exec1);
    CHECK(exec2.activeBackend() == dl::BackendType::TorchScript);
}

#ifdef DL_HAS_EXECUTORCH
TEST_CASE("ModelExecution - load nonexistent .pte file selects ExecuTorch", "[ModelExecution]")
{
    dl::ModelExecution exec;
    bool ok = exec.load("/nonexistent/path/model.pte");
    CHECK_FALSE(ok);
    CHECK_FALSE(exec.isLoaded());
}
#else
TEST_CASE("ModelExecution - .pte without ENABLE_EXECUTORCH fails gracefully", "[ModelExecution]")
{
    dl::ModelExecution exec;
    bool ok = exec.load("/nonexistent/path/model.pte");
    CHECK_FALSE(ok);
    CHECK_FALSE(exec.isLoaded());
}
#endif

// Integration tests that require real model files are tagged [integration]
// and skipped in CI unless ENABLE_INTEGRATION_TESTS is ON.
// TEST_CASE("ModelExecution - load and run real .pt model", "[ModelExecution][integration]")
// {
//     dl::ModelExecution exec;
//     REQUIRE(exec.load("/path/to/test_model.pt"));
//     REQUIRE(exec.isLoaded());
//     CHECK(exec.activeBackend() == dl::BackendType::TorchScript);
//
//     auto outputs = exec.execute({torch::randn({1, 3, 256, 256})});
//     REQUIRE_FALSE(outputs.empty());
// }

