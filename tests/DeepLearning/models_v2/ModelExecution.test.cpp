#include <catch2/catch_test_macros.hpp>

#include "models_v2/ModelExecution.hpp"

#include <torch/torch.h>

#include <filesystem>
#include <fstream>

TEST_CASE("ModelExecution - default construction", "[ModelExecution]")
{
    dl::ModelExecution exec;
    CHECK_FALSE(exec.isLoaded());
    CHECK(exec.loadedPath().empty());
}

TEST_CASE("ModelExecution - load nonexistent file", "[ModelExecution]")
{
    dl::ModelExecution exec;
    bool ok = exec.load("/nonexistent/path/model.pte");
    CHECK_FALSE(ok);
    CHECK_FALSE(exec.isLoaded());
}

TEST_CASE("ModelExecution - load invalid file", "[ModelExecution]")
{
    // Create a temp file with garbage content
    auto tmp = std::filesystem::temp_directory_path() / "test_invalid.pte";
    {
        std::ofstream f(tmp, std::ios::binary);
        f << "this is not a valid pte file";
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
    // Even if loaded, missing slot name should throw
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

// Integration tests that require a real .pte file are tagged [integration]
// and skipped in CI unless ENABLE_INTEGRATION_TESTS is ON.
// TEST_CASE("ModelExecution - load and run real model", "[ModelExecution][integration]")
// {
//     dl::ModelExecution exec;
//     REQUIRE(exec.load("/path/to/test_model.pte"));
//     REQUIRE(exec.isLoaded());
//
//     auto outputs = exec.execute({torch::randn({1, 3, 256, 256})});
//     REQUIRE_FALSE(outputs.empty());
// }
