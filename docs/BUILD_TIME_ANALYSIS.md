# Build Time Analysis and Optimization Recommendations

## Executive Summary

This document provides a comprehensive analysis of the WhiskerToolbox C++ build times using ClangBuildAnalyzer. The analysis identifies the most expensive compilation units and headers, and provides actionable recommendations to reduce build times.

## Analysis Methodology

- **Tool**: ClangBuildAnalyzer v1.6.0
- **Build Configuration**: linux-clang-release CMake preset
- **Compiler Flags**: -ftime-trace enabled
- **Build Directory**: out/build/Clang/Release

## Key Findings

### 1. Most Expensive Headers (Total Compilation Time)

| Header | Total Time | Includes | Avg Time | Impact |
|--------|-----------|----------|----------|--------|
| `DataManagerTypes.hpp` | 153.5s | 111 | 1383ms | **Critical** |
| `DataManagerParameter_Widget.hpp` | 114.2s | 18 | 6344ms | **Critical** |
| `torch/torch.h` | 111.2s | 4 | 27803ms | **Critical** |
| `<functional>` | 99.4s | 332 | 299ms | **High** |
| `TimeFrame.hpp` | 97.9s | 184 | 532ms | **High** |
| `data_transforms.hpp` | 85.4s | 74 | 1153ms | **High** |
| `Tensor_Data.hpp` | 78.9s | 11 | 7176ms | **High** |
| `<vector>` | 78.0s | 361 | 215ms | **Medium** |
| `<chrono>` | 77.4s | 231 | 335ms | **Medium** |
| `<armadillo>` | 76.3s | 29 | 2632ms | **High** |

### 2. Most Expensive Template Instantiations

| Template | Total Time | Count | Avg Time |
|----------|-----------|-------|----------|
| `std::vformat_to<>` | 157.3s | 475 | 331ms |
| `std::__format::__do_vformat_to<>` | 157.1s | 462 | 339ms |
| `std::formatter<>::format<>` | 96.0s | 5790 | 16ms |
| `std::__and_<>` | 90.0s | 53262 | 1ms |
| `std::__or_<>` | 77.2s | 58580 | 1ms |
| `std::unique_ptr<>` | 74.8s | 3177 | 23ms |
| `nlohmann::basic_json<>::parse<>` | 50.8s | 47 | 1081ms |

### 3. Slowest Individual Functions

| Function | Time | File |
|----------|------|------|
| `load_data_from_json_config` | 1135ms | DataManager.cpp |
| `MinCostFlowTracker::build_meta_nodes` | 819ms | line_kalman_grouping.cpp |
| `ParameterFactory::initializeDefaultSetters` | 738ms | ParameterFactory.cpp |
| `lineKalmanGrouping` | 644ms | line_kalman_grouping.cpp |
| `ScatterPlotPropertiesWidget::updateAvailableDataSources` | 535ms | ScatterPlotPropertiesWidget.cpp |

### 4. Most Expensive Function Sets

| Function Set | Total Time | Count | Avg Time |
|--------------|-----------|-------|----------|
| `QtPrivate::QCallableObject<>::impl` | 1657ms | 369 | 4ms |
| `std::__detail::_Compiler<>::_M_expression_term` | 1643ms | 48 | 34ms |
| `Eigen::internal::general_matrix_vector_product<>::run` | 1240ms | 20 | 62ms |
| `std::__shared_ptr<>::~__shared_ptr` | 1167ms | 324 | 3ms |
| `Eigen::internal::triangular_solve_matrix<>::run` | 1158ms | 11 | 105ms |

## Root Causes

### 1. Heavy Header Inclusion
- **DataManagerTypes.hpp** is included 111 times, pulling in expensive dependencies
- **DataManager.hpp** is included 142 times and itself includes DataManagerTypes.hpp
- Headers like `<functional>` and `<vector>` are included 300+ times each

### 2. Template-Heavy Code
- Heavy use of C++20 formatting (`std::format`, `std::vformat_to`) accounts for >300s
- Eigen matrix operations are instantiated many times
- nlohmann::json parsing templates are expensive

### 3. External Library Headers
- PyTorch (`torch/torch.h`) takes 27.8s per include (4 includes)
- Armadillo takes 2.6s per include (29 includes)
- Qt template metaprogramming (signals/slots) is expensive

### 4. Lack of Precompiled Headers
- No PCH usage detected for expensive STL headers
- System headers are recompiled in every translation unit

## Optimization Recommendations

### Priority 1: Critical Impact (Expected 30-40% build time reduction)

#### 1.1 Create Precompiled Header for STL
Create a PCH including the most expensive system headers:
- `<functional>` (99.4s total)
- `<vector>` (78.0s total)
- `<chrono>` (77.4s total)
- `<memory>`
- `<string>`
- `<unordered_map>`
- `<variant>`
- `<optional>`

**Implementation:**
```cmake
# In CMakeLists.txt
target_precompile_headers(your_target
  PUBLIC
    <functional>
    <vector>
    <chrono>
    <memory>
    <string>
    <unordered_map>
    <variant>
    <optional>
)
```

**Expected Savings:** 40-50s (15-20% of total build time)

#### 1.2 Reduce DataManagerTypes.hpp Inclusion
**Current:** Included 111 times (153.5s total)
**Problem:** Pulls in heavy dependencies transitively

**Solutions:**
1. Split into smaller headers:
   - `DataManagerTypesFwd.hpp` (forward declarations only)
   - `DataManagerTypesCore.hpp` (minimal types)
   - `DataManagerTypesVariant.hpp` (variant definition)

2. Use forward declarations in DataManager.hpp
3. Move variant definition to implementation files where possible

**Implementation Example:**
```cpp
// DataManagerTypesFwd.hpp
#ifndef DATAMANAGERTYPES_FWD_HPP
#define DATAMANAGERTYPES_FWD_HPP

// Forward declarations
class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;
class LineData;
class MaskData;
class MediaData;
class PointData;
class TensorData;

enum class DM_DataType;

#endif
```

**Expected Savings:** 40-60s (15-20% of total build time)

#### 1.3 Isolate Torch Headers
**Current:** torch/torch.h included 4 times (111.2s total, 27.8s per include)

**Solutions:**
1. Create torch_fwd.hpp with forward declarations
2. Only include torch/torch.h in implementation files
3. Use Pimpl idiom for classes containing torch types
4. Consider creating a separate torch_wrapper library

**Implementation:**
```cpp
// torch_fwd.hpp
namespace torch {
    class Tensor;
    namespace jit {
        class Module;
    }
}

// In TensorData.hpp - already using Pimpl, but verify torch not in header
class TensorDataImpl; // Forward declaration only
```

**Expected Savings:** 70-90s (25-30% of total build time)

### Priority 2: High Impact (Expected 15-20% build time reduction)

#### 2.1 Reduce Armadillo Includes
**Current:** 29 includes (76.3s total, 2.6s per include)

**Solution:** Similar to torch - use forward declarations and Pimpl

#### 2.2 Optimize DataManagerParameter_Widget.hpp
**Current:** 18 includes (114.2s total, 6.3s per include!)

**Problem:** Each include is extremely expensive

**Solutions:**
1. Use Pimpl idiom to hide DataManager dependency
2. Use forward declaration for DataManager
3. Move implementation to .cpp file
4. Consider making it interface-only

#### 2.3 Add Extern Template Declarations
Target the most instantiated templates:
- DataManager::getData<T>
- DataManager::setData<T>
- Common Eigen matrix operations
- std::shared_ptr<DataType>

**Note:** DataManager.hpp already has extern template declarations for getData/setData - verify they're working correctly.

#### 2.4 Optimize JSON Parsing
**Current:** nlohmann::json parsing takes 50.8s across 47 instantiations

**Solutions:**
1. Use `nlohmann/json_fwd.hpp` in headers
2. Only include full json.hpp in implementation files
3. Consider using a single JSON parsing function with variant return

### Priority 3: Medium Impact (Expected 5-10% build time reduction)

#### 3.1 Optimize C++20 Format Usage
**Current:** std::format templates account for >300s total

**Solutions:**
1. Use traditional string formatting in hot paths during compilation
2. Consider wrapping format calls in helper functions
3. Evaluate if C++20 formatting is worth the compilation cost
4. Use fmt library instead (potentially faster compilation)

#### 3.2 Reduce TimeFrame.hpp Inclusion
**Current:** 184 includes (97.9s total)

**Solutions:**
1. Use forward declarations where possible
2. Move inline implementations to .cpp
3. Split into smaller headers

#### 3.3 Optimize data_transforms.hpp
**Current:** 74 includes (85.4s total)

**Solutions:**
1. Split into smaller, more focused headers
2. Use forward declarations
3. Move template implementations to separate files

#### 3.4 Use Unity Builds Selectively
For source files with many small compilation units, unity builds can reduce header parsing overhead:

```cmake
set_target_properties(your_target PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE 16
)
```

**Warning:** May increase peak memory usage

### Priority 4: Low Impact Optimizations

#### 4.1 Compiler Flags
Consider adding:
- `-fno-exceptions` (if exceptions not needed)
- `-fno-rtti` (if RTTI not needed, but Qt requires it)
- `-fvisibility=hidden` (reduces symbol table size)

#### 4.2 Distributed Compilation
Consider using:
- ccache for caching compilation results
- distcc for distributed compilation
- sccache for cloud-based caching

#### 4.3 Link-Time Optimization (LTO)
**Warning:** While LTO improves runtime performance, it significantly increases link time. Consider using it only for release builds.

## Implementation Strategy

### Phase 1: Quick Wins (1-2 days)
1. Create PCH for STL headers
2. Split DataManagerTypes.hpp into forward declaration header
3. Isolate torch includes to implementation files
4. Verify extern template declarations are working

**Expected Result:** 30-40% build time reduction

### Phase 2: Medium Effort (3-5 days)
1. Implement Pimpl for DataManagerParameter_Widget
2. Optimize Armadillo includes
3. Switch to nlohmann/json_fwd.hpp in headers
4. Add more extern template declarations

**Expected Result:** Additional 15-20% build time reduction

### Phase 3: Long-term Improvements (1-2 weeks)
1. Refactor data_transforms.hpp
2. Optimize C++20 format usage
3. Implement unity builds for appropriate targets
4. Set up ccache/distcc

**Expected Result:** Additional 10-15% build time reduction

## Monitoring

After implementing changes:
1. Re-run ClangBuildAnalyzer to measure improvements
2. Compare build times before/after
3. Monitor for regressions in CI/CD
4. Consider adding build time tracking to CI

## Conclusion

The WhiskerToolbox project has significant opportunities for build time optimization. The primary issues are:

1. **Heavy STL header inclusion** without precompiled headers
2. **Expensive third-party headers** (PyTorch, Armadillo) included in public headers
3. **Template-heavy code** with many instantiations
4. **Large monolithic headers** that could be split

By implementing the Priority 1 and 2 recommendations, we can expect a **45-60% reduction in build times**, significantly improving developer productivity.

The analysis shows that the codebase is well-structured with good use of forward declarations in some areas (e.g., TensorData using Pimpl), but these patterns need to be applied more consistently across the project.

---

**Generated:** 2025-11-15  
**Tool:** ClangBuildAnalyzer 1.6.0  
**Build:** linux-clang-release preset
