#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "device/DeviceManager.hpp"

#include <torch/cuda.h>  // torch::cuda::is_available
#include <c10/core/DeviceType.h> // at::kCPU, at::kCUDA
#include <ATen/Functions.h> // at::randn

TEST_CASE("DeviceManager - singleton returns same instance", "[DeviceManager]")
{
    auto & dm1 = dl::DeviceManager::instance();
    auto & dm2 = dl::DeviceManager::instance();
    CHECK(&dm1 == &dm2);
}

TEST_CASE("DeviceManager - device returns valid device", "[DeviceManager]")
{
    auto & dm = dl::DeviceManager::instance();
    auto dev = dm.device();

    // On CI/test machines, CUDA may or may not be available.
    // Just verify we get a valid device type.
    CHECK((dev.type() == at::kCPU || dev.type() == at::kCUDA));
}

TEST_CASE("DeviceManager - cudaAvailable is consistent", "[DeviceManager]")
{
    auto cuda = dl::DeviceManager::cudaAvailable();
    CHECK(cuda == torch::cuda::is_available());
}

TEST_CASE("DeviceManager - setDevice overrides", "[DeviceManager]")
{
    auto & dm = dl::DeviceManager::instance();
    auto original = dm.device();

    // Force CPU
    dm.setDevice(at::Device(at::kCPU));
    CHECK(dm.device().type() == at::kCPU);

    // Restore original
    dm.setDevice(original);
    CHECK(dm.device() == original);
}

TEST_CASE("DeviceManager - toDevice moves tensor", "[DeviceManager]")
{
    auto & dm = dl::DeviceManager::instance();

    // Force CPU for reliable testing
    dm.setDevice(at::Device(at::kCPU));

    auto tensor = at::randn({2, 3});
    auto moved = dm.toDevice(tensor);

    CHECK(moved.device().type() == at::kCPU);
    CHECK(moved.sizes() == tensor.sizes());

    // Values should be identical (no copy needed when already on CPU)
    CHECK(at::allclose(moved, tensor));
}

TEST_CASE("DeviceManager - toDevice returns same tensor when already on device", "[DeviceManager]")
{
    auto & dm = dl::DeviceManager::instance();
    dm.setDevice(at::Device(at::kCPU));

    auto tensor = at::randn({4, 4});
    auto moved = dm.toDevice(tensor);

    // Should be the same tensor (same data_ptr) since it's already on CPU
    CHECK(moved.data_ptr() == tensor.data_ptr());
}
