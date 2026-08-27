#!/usr/bin/env python3
"""
Ad-hoc diagnostic script for debugging NaN output from an AOT Inductor model.

Tests the AOTI-compiled EfficientViT backbone with various input configurations
to isolate why inference produces NaN values when run through WhiskerToolbox.

Key hypotheses:
  1. Input size mismatch (model compiled for 256x256, but fed 224x224)
  2. Normalization mismatch (model expects ImageNet normalization, but gets [0,1])
  3. Model produces NaN for some input range

Usage:
    python benchmark/adhoc/debug_aoti_nan.py
"""

import sys
import numpy as np

try:
    import torch
    import torch._inductor.codecache  # Force import (workaround for lazy-import bug)
except ImportError:
    print("ERROR: PyTorch is required. Install with: pip install torch")
    sys.exit(1)


MODEL_PATH = "/home/wanglab/Downloads/efficientvit_cpu_aoti.pt2"

# ImageNet normalization constants
IMAGENET_MEAN = [0.485, 0.456, 0.406]
IMAGENET_STD = [0.229, 0.224, 0.225]


def check_for_nan(tensor: torch.Tensor, label: str) -> bool:
    """Check if tensor contains NaN/Inf and report."""
    has_nan = torch.isnan(tensor).any().item()
    has_inf = torch.isinf(tensor).any().item()
    if has_nan or has_inf:
        nan_count = torch.isnan(tensor).sum().item()
        inf_count = torch.isinf(tensor).sum().item()
        total = tensor.numel()
        print(f"  [FAIL] {label}: {nan_count} NaN, {inf_count} Inf out of {total} elements")
        return True
    else:
        vmin = tensor.min().item()
        vmax = tensor.max().item()
        vmean = tensor.mean().item()
        print(f"  [OK]   {label}: min={vmin:.6f}, max={vmax:.6f}, mean={vmean:.6f}, shape={list(tensor.shape)}")
        return False


def load_model(path: str):
    """Load the AOTI model package."""
    print(f"\n{'='*70}")
    print(f"Loading AOTI model from: {path}")
    print(f"{'='*70}")
    try:
        model = torch._inductor.aoti_load_package(path)
        print("  Model loaded successfully")
        return model
    except Exception as e:
        print(f"  ERROR loading model: {e}")
        sys.exit(1)


def test_input_size(model, batch_size: int, height: int, width: int, label: str):
    """Test model with specific input dimensions."""
    print(f"\n--- Test: {label} (batch={batch_size}, {height}x{width}) ---")
    try:
        x = torch.randn(batch_size, 3, height, width)
        print(f"  Input: shape={list(x.shape)}, dtype={x.dtype}")
        with torch.no_grad():
            out = model(x)
        if isinstance(out, (tuple, list)):
            for i, o in enumerate(out):
                check_for_nan(o, f"output[{i}]")
        else:
            check_for_nan(out, "output")
        return True
    except Exception as e:
        print(f"  [ERROR] {e}")
        return False


def test_normalization(model, height: int, width: int):
    """Test different normalization strategies."""
    print(f"\n{'='*70}")
    print("Testing normalization strategies")
    print(f"{'='*70}")

    batch_size = 2

    # 1. Raw uint8-style [0, 255] → no normalization
    print("\n--- Test: Raw [0, 255] (no normalization) ---")
    x = torch.randint(0, 256, (batch_size, 3, height, width)).float()
    with torch.no_grad():
        out = model(x)
    if isinstance(out, (tuple, list)):
        out = out[0]
    check_for_nan(out, "raw_0_255")

    # 2. Simple [0, 1] normalization (what WhiskerToolbox ImageEncoder does)
    print("\n--- Test: Simple [0, 1] normalization ---")
    x = torch.randint(0, 256, (batch_size, 3, height, width)).float() / 255.0
    with torch.no_grad():
        out = model(x)
    if isinstance(out, (tuple, list)):
        out = out[0]
    check_for_nan(out, "simple_0_1")

    # 3. ImageNet normalization (mean/std)
    print("\n--- Test: ImageNet normalization ---")
    x = torch.randint(0, 256, (batch_size, 3, height, width)).float() / 255.0
    mean = torch.tensor(IMAGENET_MEAN).view(1, 3, 1, 1)
    std = torch.tensor(IMAGENET_STD).view(1, 3, 1, 1)
    x = (x - mean) / std
    with torch.no_grad():
        out = model(x)
    if isinstance(out, (tuple, list)):
        out = out[0]
    check_for_nan(out, "imagenet_norm")

    # 4. Standard normal (randn)
    print("\n--- Test: Standard normal (randn) ---")
    x = torch.randn(batch_size, 3, height, width)
    with torch.no_grad():
        out = model(x)
    if isinstance(out, (tuple, list)):
        out = out[0]
    check_for_nan(out, "randn")

    # 5. All zeros
    print("\n--- Test: All zeros ---")
    x = torch.zeros(batch_size, 3, height, width)
    with torch.no_grad():
        out = model(x)
    if isinstance(out, (tuple, list)):
        out = out[0]
    check_for_nan(out, "all_zeros")

    # 6. All ones
    print("\n--- Test: All ones ---")
    x = torch.ones(batch_size, 3, height, width)
    with torch.no_grad():
        out = model(x)
    if isinstance(out, (tuple, list)):
        out = out[0]
    check_for_nan(out, "all_ones")


def test_global_avg_pool(model, height: int, width: int):
    """Simulate the GlobalAvgPoolModule applied in WhiskerToolbox."""
    print(f"\n{'='*70}")
    print("Testing Global Average Pooling (simulating WhiskerToolbox pipeline)")
    print(f"{'='*70}")

    batch_size = 2
    x = torch.randn(batch_size, 3, height, width)
    print(f"  Input: shape={list(x.shape)}")

    with torch.no_grad():
        features = model(x)

    if isinstance(features, (tuple, list)):
        features = features[0]

    print(f"  Encoder output: shape={list(features.shape)}")
    check_for_nan(features, "encoder_output")

    if features.dim() == 4:
        pooled = torch.nn.functional.adaptive_avg_pool2d(features, (1, 1))
        pooled = pooled.squeeze(-1).squeeze(-1)
        print(f"  After GlobalAvgPool: shape={list(pooled.shape)}")
        check_for_nan(pooled, "after_global_avg_pool")

        # Convert to numpy (simulating TensorToFeatureVector decode)
        for b in range(batch_size):
            row = pooled[b].cpu().float().numpy()
            nan_count = np.isnan(row).sum()
            print(f"  Batch {b} feature vector: len={len(row)}, NaN count={nan_count}")
    elif features.dim() == 2:
        print("  Output is already [B, C] — no spatial pooling needed")
        check_for_nan(features, "features_2d")


def test_size_mismatch(model, compiled_size: int):
    """Test what happens when wrong input size is fed."""
    print(f"\n{'='*70}")
    print(f"Testing input size mismatch (compiled for {compiled_size}x{compiled_size})")
    print(f"{'='*70}")

    wrong_sizes = [224, 256, 512, 128]
    for size in wrong_sizes:
        label = f"{'CORRECT' if size == compiled_size else 'WRONG'} {size}x{size}"
        test_input_size(model, batch_size=2, height=size, width=size, label=label)


def main():
    model = load_model(MODEL_PATH)

    # The model was compiled with 256x256 input
    compiled_h, compiled_w = 256, 256

    print(f"\n{'='*70}")
    print("Test 1: Basic inference with compiled input size")
    print(f"{'='*70}")
    test_input_size(model, batch_size=1, height=compiled_h, width=compiled_w, label="single_batch")
    test_input_size(model, batch_size=2, height=compiled_h, width=compiled_w, label="batch_2")

    # Test 2: What happens with wrong sizes
    test_size_mismatch(model, compiled_size=compiled_h)

    # Test 3: Normalization variants
    test_normalization(model, height=compiled_h, width=compiled_w)

    # Test 4: Full pipeline simulation (encoder → global avg pool → feature vector)
    test_global_avg_pool(model, height=compiled_h, width=compiled_w)

    print(f"\n{'='*70}")
    print("SUMMARY")
    print(f"{'='*70}")
    print(f"""
Key findings to check:
  - If 'WRONG 224x224' fails but 'CORRECT 256x256' works:
    → WhiskerToolbox was sending the wrong input size.
    → Fix: Set encoder input height/width to {compiled_h} in the Encoder Shape section.

  - If 'simple_0_1' shows NaN but 'imagenet_norm' is fine:
    → The model expects ImageNet normalization, not just [0,1].
    → The ImageEncoder only divides by 255 but doesn't subtract mean/divide by std.

  - If ALL outputs show NaN:
    → The AOTI model itself may have compilation issues. Try re-exporting.

  - If encoder output is fine but GlobalAvgPool shows NaN:
    → Edge case in the pooling operation (unlikely).
""")


if __name__ == "__main__":
    main()
