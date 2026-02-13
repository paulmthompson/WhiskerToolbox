# Quick Start: Enable Precompiled Headers

This guide will help you enable precompiled headers (PCH) in 5 minutes for an immediate 15-20% build time improvement.

## What Are Precompiled Headers?

Precompiled headers allow the compiler to compile expensive headers once and reuse them across all translation units, instead of recompiling them every time.

## Expected Impact

- **Build time reduction:** 15-20% (40-50 seconds for WhiskerToolbox)
- **Implementation time:** 5 minutes
- **Risk:** Very low - PCH is a standard CMake feature

## Step 1: Update Main CMakeLists.txt

Add this line to `/home/runner/work/WhiskerToolbox/WhiskerToolbox/CMakeLists.txt` after the other includes (around line 111):

```cmake
include(enable_sanitizers)
include(CompilerWarnings)
include(set_rpath)
include(iwyu)
include(DataManagerUtils)
include(EnablePCH)  # <-- ADD THIS LINE
```

## Step 2: Enable PCH for DataManager Library

Edit `/home/runner/work/WhiskerToolbox/WhiskerToolbox/src/DataManager/CMakeLists.txt` and add at the end:

```cmake
# Enable precompiled headers for build time optimization
# See docs/BUILD_TIME_ANALYSIS.md for details
enable_whiskertoolbox_pch(DataManager)
```

## Step 3: Enable PCH for WhiskerToolbox (if UI enabled)

Edit `/home/runner/work/WhiskerToolbox/WhiskerToolbox/src/WhiskerToolbox/CMakeLists.txt` and add at the end:

```cmake
# Enable precompiled headers for build time optimization
enable_whiskertoolbox_pch(WhiskerToolbox)
```

## Step 4: Rebuild

```bash
# Clean build to measure improvement
rm -rf out/build/Clang/Release
cmake --preset linux-clang-release
cmake --build --preset linux-clang-release
```

## Step 5: Measure Improvement (Optional)

If you want to measure the exact improvement:

```bash
# Rebuild with time tracking
time cmake --build --preset linux-clang-release --clean-first

# Or use ClangBuildAnalyzer again
cd out/build/Clang/Release
/path/to/ClangBuildAnalyzer --all . /tmp/build_with_pch.bin
/path/to/ClangBuildAnalyzer --analyze /tmp/build_with_pch.bin
```

## What Gets Precompiled?

The PCH includes these expensive STL headers (based on build analysis):

```cpp
// Standard library containers
#include <vector>        // 78.0s total cost without PCH
#include <string>        // Heavily used
#include <map>
#include <unordered_map>
#include <set>

// Standard library utilities  
#include <memory>        // Used in shared_ptr
#include <functional>    // 99.4s total cost without PCH
#include <optional>
#include <variant>       // Very expensive!
#include <chrono>        // 77.4s total cost without PCH

// Standard library algorithms
#include <algorithm>
#include <iterator>

// And more...
```

These headers are compiled once and reused across all source files.

## Troubleshooting

### PCH Not Working?

Check your CMake version:
```bash
cmake --version  # Need 3.16+
```

### Want to Disable PCH?

Add to cmake command:
```bash
cmake --preset linux-clang-release -DDISABLE_PCH=ON
```

### Different Compiler?

The PCH file works with GCC, Clang, and MSVC. CMake handles the differences automatically.

## What's Next?

After enabling PCH, you can:

1. **Immediate benefit:** Enjoy 15-20% faster builds
2. **Next optimization:** Use DataManagerTypesFwd.hpp (see DATAMANAGERTYPES_OPTIMIZATION.md)
3. **Big impact:** Isolate torch headers (see TORCH_OPTIMIZATION.md)

## Technical Details

The `enable_whiskertoolbox_pch()` function:
- Is defined in `cmake/EnablePCH.cmake`
- Uses CMake's `target_precompile_headers()` 
- Only applies to specified target, not globally
- Creates `.pch` or `.gch` files in build directory
- Automatically handles dependencies

## FAQ

**Q: Will this increase my binary size?**  
A: No, PCH only affects compilation, not linking.

**Q: Does this affect debug builds?**  
A: Yes, PCH works for all build types.

**Q: Can I customize what's in the PCH?**  
A: Yes, edit `src/whiskertoolbox_pch.hpp` to add/remove headers.

**Q: Will incremental builds be slower?**  
A: No - incremental builds will be faster because unchanged headers don't need recompiling.

**Q: Does this work with ccache?**  
A: Yes, modern ccache versions support PCH.

## Summary

- ✅ 5 minutes to implement
- ✅ 15-20% build time improvement
- ✅ No code changes required
- ✅ Low risk
- ✅ Works with all compilers

**This is the single highest ROI optimization you can do!**

---

For more advanced optimizations, see:
- docs/BUILD_TIME_ANALYSIS.md - Complete analysis
- docs/DATAMANAGERTYPES_OPTIMIZATION.md - Header splitting strategy
- docs/TORCH_OPTIMIZATION.md - PyTorch header isolation
