/// Standalone test driver for NeuroSAM model inference debugging.
/// Loads weights and runs forward pass with dummy inputs.

#include "models_v2/neurosam/NeuroSAMModel.hpp"
#include "device/DeviceManager.hpp"

#include <torch/torch.h>

#include <chrono>
#include <iostream>
#include <filesystem>
#include <stdexcept>

int main(int argc, char** argv) {
    std::cout << "=== NeuroSAM Test Driver ===\n\n";

    // ── Parse arguments ──
    std::filesystem::path weights_path = "/home/wanglab/Downloads/extra_random_model.pt2";
    if (argc > 1) {
        weights_path = argv[1];
    }

    std::cout << "Weights path: " << weights_path << "\n";
    if (!std::filesystem::exists(weights_path)) {
        std::cerr << "ERROR: Weights file does not exist!\n";
        return 1;
    }

    // ── Device info ──
    auto& dev_mgr = dl::DeviceManager::instance();
    std::cout << "Device: " << dev_mgr.device() << "\n";
    std::cout << "CUDA available: " << (dev_mgr.cudaAvailable() ? "yes" : "no") << "\n\n";

    // ── Create model ──
    std::cout << "Creating NeuroSAMModel...\n";
    dl::NeuroSAMModel model;
    
    std::cout << "  Model ID: " << model.modelId() << "\n";
    std::cout << "  Display Name: " << model.displayName() << "\n";
    std::cout << "  Description: " << model.description() << "\n";
    std::cout << "  Preferred batch size: " << model.preferredBatchSize() << "\n";
    std::cout << "  Max batch size: " << model.maxBatchSize() << "\n\n";

    // ── Print input slots ──
    auto input_slots = model.inputSlots();
    std::cout << "Input slots (" << input_slots.size() << "):\n";
    for (auto const& slot : input_slots) {
        std::cout << "  - " << slot.name << " | shape: [";
        for (size_t i = 0; i < slot.shape.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << slot.shape[i];
        }
        std::cout << "] | static: " << (slot.is_static ? "yes" : "no")
                  << " | boolean: " << (slot.is_boolean_mask ? "yes" : "no") << "\n";
        if (!slot.recommended_encoder.empty()) {
            std::cout << "    encoder hint: " << slot.recommended_encoder << "\n";
        }
    }
    std::cout << "\n";

    // ── Print output slots ──
    auto output_slots = model.outputSlots();
    std::cout << "Output slots (" << output_slots.size() << "):\n";
    for (auto const& slot : output_slots) {
        std::cout << "  - " << slot.name << " | shape: [";
        for (size_t i = 0; i < slot.shape.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << slot.shape[i];
        }
        std::cout << "]\n";
        if (!slot.recommended_decoder.empty()) {
            std::cout << "    decoder hint: " << slot.recommended_decoder << "\n";
        }
    }
    std::cout << "\n";

    // ── Load weights ──
    std::cout << "Loading weights from " << weights_path << "...\n";
    try {
        model.loadWeights(weights_path);
        std::cout << "  ✓ Weights loaded successfully\n";
        std::cout << "  Ready: " << (model.isReady() ? "yes" : "no") << "\n\n";
    } catch (std::exception const& e) {
        std::cerr << "  ✗ Failed to load weights: " << e.what() << "\n";
        return 1;
    }

    // ── Create dummy inputs ──
    std::cout << "Creating dummy input tensors...\n";
    std::unordered_map<std::string, torch::Tensor> inputs;

    // NOTE: AOT Inductor compiles for fixed shapes. The batch size must match
    // the batch size used during export (8 in the NeuroSAM export script).
    // Override the model's preferredBatchSize if needed.
    int const batch_size = 1;  // Must match export batch size for AOTI

    try {
        // encoder_image: [batch, 3, 256, 256] - uint8 (model expects uint8 images)
        {
            auto tensor = torch::randint(0, 255, {batch_size, 3, 256, 256}, torch::kUInt8);
            inputs["encoder_image"] = dev_mgr.toDevice(tensor);
            std::cout << "  - encoder_image: " << inputs["encoder_image"].sizes() 
                      << " dtype=" << inputs["encoder_image"].dtype()
                      << " on " << inputs["encoder_image"].device() << "\n";
        }

        // memory_images: [batch, 3, 256, 256] - uint8 (model expects uint8 images)
        {
            auto tensor = torch::randint(0, 255, {batch_size, 3, 256, 256}, torch::kUInt8);
            inputs["memory_images"] = dev_mgr.toDevice(tensor);
            std::cout << "  - memory_images: " << inputs["memory_images"].sizes() 
                      << " dtype=" << inputs["memory_images"].dtype()
                      << " on " << inputs["memory_images"].device() << "\n";
        }

        // memory_masks: [batch, 1, 256, 256] - float32 (model expects float masks)
        {
            auto tensor = torch::zeros({batch_size, 1, 256, 256}, torch::kFloat32);
            // Create a simple circular mask in the center
            for (int b = 0; b < batch_size; ++b) {
                for (int y = 100; y < 156; ++y) {
                    for (int x = 100; x < 156; ++x) {
                        float dy = static_cast<float>(y - 128);
                        float dx = static_cast<float>(x - 128);
                        if (dy*dy + dx*dx < 28*28) {
                            tensor[b][0][y][x] = 1.0f;
                        }
                    }
                }
            }
            inputs["memory_masks"] = dev_mgr.toDevice(tensor);
            std::cout << "  - memory_masks: " << inputs["memory_masks"].sizes() 
                      << " dtype=" << inputs["memory_masks"].dtype()
                      << " on " << inputs["memory_masks"].device() << "\n";
        }

        // memory_mask (boolean): [batch, 1]
        // NOTE: Uncomment if your model uses this input
        // {
        //     auto tensor = torch::ones({batch_size, 1}, torch::kFloat32);
        //     inputs["memory_mask"] = dev_mgr.toDevice(tensor);
        //     std::cout << "  - memory_mask: " << inputs["memory_mask"].sizes() 
        //               << " on " << inputs["memory_mask"].device() << "\n";
        // }

        std::cout << "\n";

    } catch (std::exception const& e) {
        std::cerr << "  ✗ Failed to create dummy inputs: " << e.what() << "\n";
        return 1;
    }

    // ── Run forward pass ──
    std::cout << "Running forward pass...\n";
    try {
        // Warmup run (first run is often slower due to lazy initialization)
        auto warmup_outputs = model.forward(inputs);
        std::cout << "  ✓ Warmup pass completed\n";

        // Timed runs
        constexpr int num_runs = 10;
        auto start = std::chrono::high_resolution_clock::now();
        
        std::unordered_map<std::string, torch::Tensor> outputs;
        for (int i = 0; i < num_runs; ++i) {
            outputs = model.forward(inputs);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto total_ms = std::chrono::duration<double, std::milli>(end - start).count();
        auto avg_ms = total_ms / num_runs;
        
        std::cout << "  ✓ Forward pass succeeded!\n";
        std::cout << "  Timing (" << num_runs << " runs): "
                  << "total=" << total_ms << "ms, "
                  << "avg=" << avg_ms << "ms/inference\n\n";

        std::cout << "Output tensors (" << outputs.size() << "):\n";
        for (auto const& [name, tensor] : outputs) {
            std::cout << "  - " << name << ": " << tensor.sizes() 
                      << " on " << tensor.device() << "\n";
            
            // Print basic statistics
            auto cpu_tensor = tensor.cpu();
            float min_val = cpu_tensor.min().item<float>();
            float max_val = cpu_tensor.max().item<float>();
            float mean_val = cpu_tensor.mean().item<float>();
            std::cout << "    min: " << min_val 
                      << ", max: " << max_val 
                      << ", mean: " << mean_val << "\n";
        }

        std::cout << "\n✓ Test completed successfully!\n";
        return 0;

    } catch (std::exception const& e) {
        std::cerr << "  ✗ Forward pass failed: " << e.what() << "\n";
        std::cerr << "\nFull error trace:\n";
        std::cerr << "  Exception type: " << typeid(e).name() << "\n";
        return 1;
    }
}
