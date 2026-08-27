/// @file debug_neurosam_gpu.cpp
/// @brief Ad-hoc diagnostic for NeuroSAM GPU AOT Inductor loading and inference.
///
/// Tests five stages independently:
///   1. CUDA / libtorch environment sanity
///   2. AOTIModelPackageLoader weight loading
///   3. Forward pass with dummy tensors (raw loader)
///   4. Multiple sequential inferences (recurrent simulation)
///   5. Application-realistic tensor flow (CPU-create → encode → cache → assemble → toDevice → run)
///
/// Build via the adhoc CMakeLists.txt target `debug_neurosam_gpu`.

#include <torch/torch.h>
#include <torch/cuda.h> // is_available, device_count, cudnn_is_available
#include <torch/csrc/inductor/aoti_package/model_package_loader.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr int kModelSize = 256;
constexpr int kImageChannels = 3;
constexpr int kMaskChannels = 1;

void printSeparator(char const * title) {
    std::cout << "\n========== " << title << " ==========\n";
}

// -----------------------------------------------------------------------
// Stage 1: Environment diagnostics
// -----------------------------------------------------------------------
bool checkEnvironment() {
    printSeparator("Stage 1: Environment");

    std::cout << "LibTorch version: " << TORCH_VERSION << "\n";
    std::cout << "CUDA available:   " << (torch::cuda::is_available() ? "yes" : "no") << "\n";
    std::cout << "CUDA device count: " << torch::cuda::device_count() << "\n";
    std::cout << "cuDNN available:  " << (torch::cuda::cudnn_is_available() ? "yes" : "no") << "\n";

    if (!torch::cuda::is_available()) {
        std::cerr << "FATAL: CUDA not available in libtorch. Cannot test GPU model.\n";
        return false;
    }

    // Smoke-test: allocate a small tensor on GPU
    try {
        auto t = torch::zeros({1, 3, 64, 64}, torch::device(torch::kCUDA).dtype(torch::kFloat32));
        std::cout << "Smoke test tensor on GPU: " << t.sizes() << " device=" << t.device() << "  OK\n";
    } catch (std::exception const & e) {
        std::cerr << "FATAL: Cannot allocate tensor on GPU: " << e.what() << "\n";
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------
// Stage 2: Load the .pt2 model package
// -----------------------------------------------------------------------
std::unique_ptr<torch::inductor::AOTIModelPackageLoader>
tryLoadModel(std::filesystem::path const & model_path, bool force_cpu) {
    printSeparator("Stage 2: Load Model");

    if (!std::filesystem::exists(model_path)) {
        std::cerr << "FATAL: Model file does not exist: " << model_path << "\n";
        return nullptr;
    }
    std::cout << "Model path: " << model_path << "\n";
    std::cout << "Model size: " << std::filesystem::file_size(model_path) << " bytes\n";

    if (force_cpu) {
        // CPU-only: try CPU first
        std::cout << "\n--- Attempting load on CPU (device_idx=-1) ---\n";
        try {
            auto loader = std::make_unique<torch::inductor::AOTIModelPackageLoader>(
                model_path.string(),
                /*model_name=*/"model",
                /*run_single_threaded=*/false,
                /*num_runners=*/1,
                /*device_idx=*/static_cast<c10::DeviceIndex>(-1));

            std::cout << "SUCCESS: Model loaded on CPU\n";
            return loader;
        } catch (c10::Error const & e) {
            std::cerr << "c10::Error on CPU load: " << e.what() << "\n";
        } catch (std::exception const & e) {
            std::cerr << "std::exception on CPU load: " << e.what() << "\n";
        }
        std::cerr << "\nFATAL: Could not load model on CPU.\n";
        return nullptr;
    }

    // Try loading on CUDA:0
    std::cout << "\n--- Attempting load on CUDA:0 (device_idx=0) ---\n";
    try {
        auto loader = std::make_unique<torch::inductor::AOTIModelPackageLoader>(
            model_path.string(),
            /*model_name=*/"model",
            /*run_single_threaded=*/false,
            /*num_runners=*/1,
            /*device_idx=*/static_cast<c10::DeviceIndex>(0));

        std::cout << "SUCCESS: Model loaded on CUDA:0\n";
        return loader;
    } catch (c10::Error const & e) {
        std::cerr << "c10::Error on CUDA:0 load: " << e.what() << "\n";
    } catch (std::exception const & e) {
        std::cerr << "std::exception on CUDA:0 load: " << e.what() << "\n";
    }

    // Fallback: try loading on CPU
    std::cout << "\n--- Attempting load on CPU (device_idx=-1) ---\n";
    try {
        auto loader = std::make_unique<torch::inductor::AOTIModelPackageLoader>(
            model_path.string(),
            /*model_name=*/"model",
            /*run_single_threaded=*/false,
            /*num_runners=*/1,
            /*device_idx=*/static_cast<c10::DeviceIndex>(-1));

        std::cout << "SUCCESS: Model loaded on CPU\n";
        std::cout << "NOTE: The model was exported as a GPU model but only loads on CPU.\n"
                  << "      This likely indicates a version mismatch between the exporting\n"
                  << "      Python torch and the C++ libtorch, or CUDA toolkit incompatibility.\n";
        return loader;
    } catch (c10::Error const & e) {
        std::cerr << "c10::Error on CPU load: " << e.what() << "\n";
    } catch (std::exception const & e) {
        std::cerr << "std::exception on CPU load: " << e.what() << "\n";
    }

    std::cerr << "\nFATAL: Could not load model on any device.\n";
    return nullptr;
}

// -----------------------------------------------------------------------
// Stage 3: Inference with various dtype/batch combos
// -----------------------------------------------------------------------
void tryInference(torch::inductor::AOTIModelPackageLoader & loader,
                  bool use_gpu) {
    printSeparator("Stage 3: Inference (raw loader)");

    auto const device = use_gpu ? torch::kCUDA : torch::kCPU;
    int const batch_size = 1;

    std::cout << "Creating dummy inputs (batch=" << batch_size << ") on " << device << "...\n";

    // ── Attempt 1: CPU tensors → moved to GPU (simulates validateWeights) ──
    std::cout << "\n--- Attempt 1: CPU-created tensors moved to GPU ---\n";
    {
        auto encoder_image = torch::zeros({batch_size, kImageChannels, kModelSize, kModelSize},
                                          torch::dtype(torch::kUInt8));
        auto memory_images = torch::zeros({batch_size, kImageChannels, kModelSize, kModelSize},
                                          torch::dtype(torch::kUInt8));
        auto memory_masks = torch::zeros({batch_size, kMaskChannels, kModelSize, kModelSize},
                                         torch::dtype(torch::kFloat32));

        // Move to device (simulates DeviceManager::toDevice)
        encoder_image = encoder_image.to(device);
        memory_images = memory_images.to(device);
        memory_masks = memory_masks.to(device);

        std::cout << "  encoder_image: " << encoder_image.sizes()
                  << " dtype=" << encoder_image.dtype()
                  << " device=" << encoder_image.device() << "\n";

        std::vector<at::Tensor> inputs = {encoder_image, memory_images, memory_masks};
        try {
            auto start = std::chrono::steady_clock::now();
            auto outputs = loader.run(inputs);
            auto elapsed = std::chrono::steady_clock::now() - start;
            std::cout << "SUCCESS! " << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
                      << " ms, output: " << outputs[0].sizes() << "\n";
        } catch (std::exception const & e) {
            std::cerr << "FAILED: " << e.what() << "\n";
        }
    }

    // ── Attempt 2: Direct GPU float32 tensors ──
    std::cout << "\n--- Attempt 2: Direct GPU float32 ---\n";
    {
        auto encoder_image = torch::randn({batch_size, kImageChannels, kModelSize, kModelSize},
                                          torch::device(device).dtype(torch::kFloat32));
        auto memory_images = torch::randn({batch_size, kImageChannels, kModelSize, kModelSize},
                                          torch::device(device).dtype(torch::kFloat32));
        auto memory_masks = torch::randn({batch_size, kMaskChannels, kModelSize, kModelSize},
                                         torch::device(device).dtype(torch::kFloat32));

        std::vector<at::Tensor> inputs = {encoder_image, memory_images, memory_masks};
        try {
            auto start = std::chrono::steady_clock::now();
            auto outputs = loader.run(inputs);
            auto elapsed = std::chrono::steady_clock::now() - start;
            std::cout << "SUCCESS! " << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
                      << " ms, output: " << outputs[0].sizes() << "\n";
        } catch (std::exception const & e) {
            std::cerr << "FAILED: " << e.what() << "\n";
        }
    }

    // ── Attempt 3: batch=2 float32 ──
    std::cout << "\n--- Attempt 3: batch=2, float32 ---\n";
    {
        int const b = 2;
        auto enc = torch::randn({b, kImageChannels, kModelSize, kModelSize},
                                torch::device(device).dtype(torch::kFloat32));
        auto mem = torch::randn({b, kImageChannels, kModelSize, kModelSize},
                                torch::device(device).dtype(torch::kFloat32));
        auto msk = torch::randn({b, kMaskChannels, kModelSize, kModelSize},
                                torch::device(device).dtype(torch::kFloat32));

        std::vector<at::Tensor> inputs = {enc, mem, msk};
        try {
            auto outputs = loader.run(inputs);
            std::cout << "SUCCESS! output: " << outputs[0].sizes() << "\n";
        } catch (std::exception const & e) {
            std::cerr << "FAILED: " << e.what() << "\n";
        }
    }
}

// -----------------------------------------------------------------------
// Stage 4: Recurrent simulation (feedback loop)
// -----------------------------------------------------------------------
void tryRecurrentInference(torch::inductor::AOTIModelPackageLoader & loader,
                           bool use_gpu) {
    printSeparator("Stage 4: Recurrent Simulation (5 frames)");

    auto const device = use_gpu ? torch::kCUDA : torch::kCPU;
    int const num_frames = 5;

    // Initial memory mask = zeros (simulates RecurrentInitMode::Zeros)
    auto recurrent_mask = torch::zeros(
        {1, kMaskChannels, kModelSize, kModelSize},
        torch::device(device).dtype(torch::kFloat32));

    // Static memory image (stays the same for all frames)
    auto memory_image = torch::randint(0, 255,
        {1, kImageChannels, kModelSize, kModelSize},
        torch::device(device).dtype(torch::kUInt8));

    std::cout << "Running " << num_frames << " sequential frames with feedback loop...\n";

    for (int f = 0; f < num_frames; ++f) {
        // Current frame: new random data each time
        auto encoder_image = torch::randint(0, 255,
            {1, kImageChannels, kModelSize, kModelSize},
            torch::device(device).dtype(torch::kUInt8));

        std::vector<at::Tensor> inputs = {encoder_image, memory_image, recurrent_mask};

        try {
            auto start = std::chrono::steady_clock::now();
            auto outputs = loader.run(inputs);
            auto elapsed = std::chrono::steady_clock::now() - start;

            std::cout << "  Frame " << f << ": "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
                      << " ms, output=" << outputs[0].sizes()
                      << " min=" << outputs[0].min().item<float>()
                      << " max=" << outputs[0].max().item<float>() << "\n";

            // Feed output back as the recurrent mask
            recurrent_mask = outputs[0].detach();

        } catch (std::exception const & e) {
            std::cerr << "  Frame " << f << " FAILED: " << e.what() << "\n";
            return;
        }
    }
    std::cout << "Recurrent simulation completed successfully.\n";
}

// -----------------------------------------------------------------------
// Stage 4b: Determinism test — same inputs, repeated forward passes
// -----------------------------------------------------------------------
void tryDeterminism(torch::inductor::AOTIModelPackageLoader & loader,
                    bool use_gpu) {
    printSeparator("Stage 4b: Determinism Test (same inputs, 5 runs)");

    auto const device = use_gpu ? torch::kCUDA : torch::kCPU;

    // Create FIXED inputs (seeded, not random each time)
    torch::manual_seed(42);
    auto encoder_image = torch::randint(0, 255,
        {1, kImageChannels, kModelSize, kModelSize},
        torch::device(device).dtype(torch::kUInt8));
    auto memory_image = torch::randint(0, 255,
        {1, kImageChannels, kModelSize, kModelSize},
        torch::device(device).dtype(torch::kUInt8));
    auto memory_mask = torch::zeros(
        {1, kMaskChannels, kModelSize, kModelSize},
        torch::device(device).dtype(torch::kFloat32));

    // Run the SAME inputs multiple times and compare outputs
    torch::Tensor first_output;
    for (int run = 0; run < 5; ++run) {
        // Clone inputs so we can detect if the model mutates them
        auto enc_clone = encoder_image.clone();
        auto mem_clone = memory_image.clone();
        auto msk_clone = memory_mask.clone();

        std::vector<at::Tensor> inputs = {enc_clone, mem_clone, msk_clone};

        try {
            auto outputs = loader.run(inputs);
            auto const & out = outputs[0];

            // Check if inputs were mutated
            bool enc_same = torch::equal(enc_clone, encoder_image);
            bool mem_same = torch::equal(mem_clone, memory_image);
            bool msk_same = torch::equal(msk_clone, memory_mask);

            std::cout << "  Run " << run << ": "
                      << "min=" << out.min().item<float>()
                      << " max=" << out.max().item<float>()
                      << " sum=" << out.sum().item<float>();

            if (!enc_same) std::cout << "  *** encoder_image MUTATED ***";
            if (!mem_same) std::cout << "  *** memory_image MUTATED ***";
            if (!msk_same) std::cout << "  *** memory_mask MUTATED ***";

            if (run == 0) {
                first_output = out.clone();
            } else {
                bool outputs_match = torch::allclose(out, first_output, 1e-6, 1e-6);
                float max_diff = (out - first_output).abs().max().item<float>();
                std::cout << "  match_run0=" << (outputs_match ? "YES" : "NO")
                          << " max_diff=" << max_diff;
            }
            std::cout << "\n";

        } catch (std::exception const & e) {
            std::cerr << "  Run " << run << " FAILED: " << e.what() << "\n";
            return;
        }
    }
}

// -----------------------------------------------------------------------
// Helper: print detailed tensor properties for debugging
// -----------------------------------------------------------------------
void printTensorInfo(char const * label, torch::Tensor const & t) {
    std::cout << "  " << label << ": shape=" << t.sizes()
              << " dtype=" << t.dtype()
              << " device=" << t.device()
              << " contiguous=" << (t.is_contiguous() ? "yes" : "NO")
              << " strides=" << t.strides()
              << "\n";
}

// -----------------------------------------------------------------------
// Stage 5: Application-realistic tensor flow
// -----------------------------------------------------------------------
// Simulates exactly what SlotAssembler + NeuroSAMModel::forward() do:
//   1. Create tensors on CPU with zeros (like assembleInputs)
//   2. Fill with data (simulating ImageEncoder/Mask2DEncoder encoding)
//   3. Cache static tensors (simulating captureStaticInput)
//   4. Assemble inputs from cache (simulating assembleInputs non-sequence path)
//   5. Move to device (simulating NeuroSAMModel::forward → DeviceManager)
//   6. Move to device AGAIN (simulating AOTInductorBackend::execute → DeviceManager)
//   7. Run the model
void tryAppRealisticInference(torch::inductor::AOTIModelPackageLoader & loader,
                              bool use_gpu) {
    printSeparator("Stage 5: Application-Realistic Tensor Flow");

    auto const target_device = use_gpu ? torch::kCUDA : torch::kCPU;
    std::cout << "Target device: " << target_device << "\n";

    // ── Step 1: Simulate captureStaticInput for memory_images ──
    // captureStaticInput creates {1, C, H, W} tensor with slot dtype on CPU
    std::cout << "\n--- Step 1: captureStaticInput simulation ---\n";
    auto static_memory_image = torch::zeros(
            {1, kImageChannels, kModelSize, kModelSize}, torch::kByte);
    // Simulate ImageEncoder uint8 path: fill with pixel-like values
    // (ImageEncoder takes raw pixels, permutes to CHW, optionally resizes)
    {
        auto accessor = static_memory_image.accessor<uint8_t, 4>();
        for (int c = 0; c < kImageChannels; ++c) {
            for (int h = 0; h < kModelSize; ++h) {
                for (int w = 0; w < kModelSize; ++w) {
                    accessor[0][c][h][w] = static_cast<uint8_t>((h + w + c * 50) % 256);
                }
            }
        }
    }
    printTensorInfo("static_memory_image (cached)", static_memory_image);

    auto static_memory_mask = torch::zeros(
            {1, kMaskChannels, kModelSize, kModelSize}, torch::kFloat32);
    // Simulate Mask2DEncoder: binary mask with some active region
    static_memory_mask[0][0].slice(0, 50, 150).slice(1, 50, 150).fill_(1.0f);
    printTensorInfo("static_memory_mask (cached)", static_memory_mask);

    // ── Step 2: Simulate assembleInputs ──
    std::cout << "\n--- Step 2: assembleInputs simulation ---\n";

    // Dynamic input: encoder_image
    auto encoder_image = torch::zeros(
            {1, kImageChannels, kModelSize, kModelSize}, torch::kByte);
    {
        auto accessor = encoder_image.accessor<uint8_t, 4>();
        for (int c = 0; c < kImageChannels; ++c) {
            for (int h = 0; h < kModelSize; ++h) {
                for (int w = 0; w < kModelSize; ++w) {
                    accessor[0][c][h][w] = static_cast<uint8_t>((h * 3 + w * 7 + c * 13) % 256);
                }
            }
        }
    }

    // Static input: memory_images — copy from cache (assembleInputs does tensor.copy_(cached))
    auto memory_images = torch::zeros(
            {1, kImageChannels, kModelSize, kModelSize}, torch::kByte);
    memory_images.copy_(static_memory_image);

    // Static input: memory_masks — copy from cache
    auto memory_masks = torch::zeros(
            {1, kMaskChannels, kModelSize, kModelSize}, torch::kFloat32);
    memory_masks.copy_(static_memory_mask);

    std::cout << "Before toDevice:\n";
    printTensorInfo("encoder_image", encoder_image);
    printTensorInfo("memory_images", memory_images);
    printTensorInfo("memory_masks", memory_masks);

    // ── Step 3: NeuroSAMModel::forward() calls DeviceManager::toDevice() ──
    std::cout << "\n--- Step 3: First toDevice (NeuroSAMModel::forward) ---\n";
    encoder_image = encoder_image.to(target_device);
    memory_images = memory_images.to(target_device);
    memory_masks = memory_masks.to(target_device);

    printTensorInfo("encoder_image", encoder_image);
    printTensorInfo("memory_images", memory_images);
    printTensorInfo("memory_masks", memory_masks);

    // ── Step 4: AOTInductorBackend::execute() calls toDevice() AGAIN ──
    std::cout << "\n--- Step 4: Second toDevice (AOTInductorBackend::execute) ---\n";
    encoder_image = encoder_image.to(target_device);
    memory_images = memory_images.to(target_device);
    memory_masks = memory_masks.to(target_device);

    printTensorInfo("encoder_image", encoder_image);
    printTensorInfo("memory_images", memory_images);
    printTensorInfo("memory_masks", memory_masks);

    // ── Step 5: Run the model ──
    std::cout << "\n--- Step 5: loader.run() ---\n";
    std::vector<at::Tensor> inputs = {encoder_image, memory_images, memory_masks};
    try {
        auto start = std::chrono::steady_clock::now();
        auto outputs = loader.run(inputs);
        auto elapsed = std::chrono::steady_clock::now() - start;
        std::cout << "SUCCESS! "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
                  << " ms\n";
        printTensorInfo("output", outputs[0]);
        std::cout << "  output min=" << outputs[0].min().item<float>()
                  << " max=" << outputs[0].max().item<float>() << "\n";
    } catch (std::exception const & e) {
        std::cerr << "FAILED: " << e.what() << "\n";
        return;
    }

    // ── Step 6: Recurrent feedback loop (2 frames) ──
    std::cout << "\n--- Step 6: Recurrent feedback (simulating runRecurrentSequence) ---\n";

    // Initialize recurrent mask as zeros on CPU (like recurrent cache init)
    auto recurrent_mask = torch::zeros(
            {1, kMaskChannels, kModelSize, kModelSize}, torch::kFloat32);
    printTensorInfo("recurrent_mask (init)", recurrent_mask);

    for (int f = 0; f < 3; ++f) {
        std::cout << "\n  Frame " << f << ":\n";

        // Assemble: create encoder_image on CPU, fill, cache-copy static inputs
        auto frame_enc = torch::zeros(
                {1, kImageChannels, kModelSize, kModelSize}, torch::kByte);
        {
            auto accessor = frame_enc.accessor<uint8_t, 4>();
            for (int c = 0; c < kImageChannels; ++c) {
                for (int h = 0; h < kModelSize; ++h) {
                    for (int w = 0; w < kModelSize; ++w) {
                        accessor[0][c][h][w] = static_cast<uint8_t>((h + w * (f + 1) + c) % 256);
                    }
                }
            }
        }
        auto frame_mem_img = torch::zeros(
                {1, kImageChannels, kModelSize, kModelSize}, torch::kByte);
        frame_mem_img.copy_(static_memory_image);

        auto frame_mem_mask = torch::zeros(
                {1, kMaskChannels, kModelSize, kModelSize}, torch::kFloat32);
        frame_mem_mask.copy_(static_memory_mask);

        // Recurrent injection: REPLACE memory_masks with recurrent cache
        // (This is what runRecurrentSequence does for whole-slot replacement)
        frame_mem_mask = recurrent_mask;

        // First toDevice (NeuroSAMModel::forward)
        frame_enc = frame_enc.to(target_device);
        frame_mem_img = frame_mem_img.to(target_device);
        frame_mem_mask = frame_mem_mask.to(target_device);

        // Second toDevice (AOTInductorBackend::execute)
        frame_enc = frame_enc.to(target_device);
        frame_mem_img = frame_mem_img.to(target_device);
        frame_mem_mask = frame_mem_mask.to(target_device);

        printTensorInfo("  encoder_image", frame_enc);
        printTensorInfo("  memory_images", frame_mem_img);
        printTensorInfo("  memory_masks", frame_mem_mask);

        std::vector<at::Tensor> frame_inputs = {frame_enc, frame_mem_img, frame_mem_mask};
        try {
            auto start = std::chrono::steady_clock::now();
            auto frame_outputs = loader.run(frame_inputs);
            auto elapsed = std::chrono::steady_clock::now() - start;
            std::cout << "  SUCCESS! "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
                      << " ms\n";
            printTensorInfo("  output", frame_outputs[0]);
            std::cout << "    min=" << frame_outputs[0].min().item<float>()
                      << " max=" << frame_outputs[0].max().item<float>() << "\n";

            // Feedback: detach output as next recurrent_mask
            // NOTE: output is on GPU! It stays on GPU for next iteration.
            // This matches how runRecurrentSequence stores it in recurrent_cache.
            recurrent_mask = frame_outputs[0].detach();
            printTensorInfo("  recurrent_mask (updated)", recurrent_mask);

        } catch (std::exception const & e) {
            std::cerr << "  Frame " << f << " FAILED: " << e.what() << "\n";
            return;
        }
    }
    std::cout << "\nApplication-realistic flow completed successfully.\n";
}

}// namespace

int main(int argc, char * argv[]) {
    std::string model_path = "/home/wanglab/Downloads/neuroSAM_gpu_aoti.pt2";
    bool force_cpu = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--cpu") {
            force_cpu = true;
        } else {
            model_path = arg;
        }
    }

    std::cout << "NeuroSAM GPU AOTI Debug Diagnostic\n"
              << "===================================\n";
    if (force_cpu) {
        std::cout << "NOTE: --cpu flag set, will use CPU device for inference\n";
    }

    // Stage 1
    if (!force_cpu && !checkEnvironment()) {
        return 1;
    }
    if (force_cpu) {
        printSeparator("Stage 1: Environment (CPU mode)");
        std::cout << "LibTorch version: " << TORCH_VERSION << "\n";
        std::cout << "Running in CPU-only mode (--cpu flag)\n";
    }

    // Stage 2
    auto loader = tryLoadModel(model_path, force_cpu);
    if (!loader) {
        std::cerr << "\n*** DIAGNOSIS ***\n"
                  << "Model loading failed. Most likely causes:\n"
                  << "  1. VERSION MISMATCH: Model was exported with Python torch 2.10.0\n"
                  << "     but C++ libtorch is " << TORCH_VERSION << ".\n"
                  << "     AOT Inductor .pt2 packages are NOT cross-version compatible.\n"
                  << "     Fix: Re-export the model using torch " << TORCH_VERSION << "\n"
                  << "     or update libtorch to match the export version.\n"
                  << "  2. CUDA toolkit mismatch between export and runtime environment.\n"
                  << "  3. Corrupted .pt2 file.\n";
        return 2;
    }

    bool const use_gpu = !force_cpu && torch::cuda::is_available();

    // Stage 3
    tryInference(*loader, use_gpu);

    // Stage 4
    tryRecurrentInference(*loader, use_gpu);

    // Stage 4b
    tryDeterminism(*loader, use_gpu);

    // Stage 5
    tryAppRealisticInference(*loader, use_gpu);

    printSeparator("Done");
    return 0;
}
