# Build Time Optimization: Isolating PyTorch Headers

## Problem
ClangBuildAnalyzer shows that `torch/torch.h` takes 27.8 seconds per include:
- Total: 111.2s across 4 includes
- Average: 27803ms per include
- This is the 3rd most expensive header in the entire project

## Current State
PyTorch headers are currently included in:
1. `Tensor_Data_LibTorch.cpp` (implementation - OK)
2. `Tensor_Data_Impl.hpp` (header - PROBLEMATIC)
3. `torch_helpers.hpp` (header - PROBLEMATIC)
4. `efficientvit.hpp` (header - PROBLEMATIC)
5. `EfficientSAM.cpp` (implementation - OK)
6. `scm.cpp` (implementation - OK)
7. `Whisker_Widget.cpp` (implementation - OK)

## Solution: Forward Declarations

### Step 1: Create torch_fwd.hpp
Create a forward declaration header for torch types:

```cpp
// src/DataManager/Tensors/torch_fwd.hpp
#ifndef TORCH_FWD_HPP
#define TORCH_FWD_HPP

// Forward declarations for PyTorch types
// Use this header instead of <torch/torch.h> when you only need
// to declare pointers or references to torch types.

namespace torch {
    class Tensor;
    
    namespace jit {
        class Module;
        class script;
    }
    
    namespace nn {
        class Module;
    }
}

namespace at {
    class Tensor;
}

namespace c10 {
    template<typename T>
    class optional;
    
    class Device;
    class ScalarType;
}

#endif // TORCH_FWD_HPP
```

### Step 2: Verify Pimpl Pattern in Tensor_Data.hpp
The header already uses Pimpl pattern correctly:

```cpp
// Forward declaration only
class TensorDataImpl;

class TensorData : public ObserverData {
    // ... interface only, no torch types exposed ...
private:
    std::unique_ptr<TensorDataImpl> pimpl;
};
```

**Status**: ✅ Already optimized - torch types are hidden in implementation

### Step 3: Refactor torch_helpers.hpp

**Current (PROBLEMATIC):**
```cpp
// torch_helpers.hpp
#include <torch/torch.h>  // 27.8 seconds!
#include <torch/script.h>

// Functions using torch types...
```

**Optimized:**
```cpp
// torch_helpers.hpp
#include "torch_fwd.hpp"

// Forward declarations for return types
namespace torch { class Tensor; }

// Declare functions with forward-declared types
torch::Tensor some_function(torch::Tensor const& input);
// ... more declarations ...

// Note: Implementations move to torch_helpers.cpp which includes full torch/torch.h
```

### Step 4: Refactor efficientvit.hpp

**Current:**
```cpp
// efficientvit.hpp
#include <torch/torch.h>  // Heavy include

class EfficientViT : public torch::nn::Module {
    // ...
};
```

**Optimized Approach A - Pimpl:**
```cpp
// efficientvit.hpp
#include "torch_fwd.hpp"
#include <memory>

class EfficientViTImpl;  // Forward declaration

class EfficientViT {
public:
    // Interface methods
    torch::Tensor forward(torch::Tensor x);
    
private:
    std::unique_ptr<EfficientViTImpl> impl;
};
```

**Optimized Approach B - Keep torch::nn::Module (if inheritance required):**
```cpp
// efficientvit.hpp - minimal version
// Only include this in implementation files
#ifdef EFFICIENTVIT_IMPLEMENTATION
#include <torch/torch.h>

class EfficientViT : public torch::nn::Module {
    // ... full definition ...
};

#else
// Forward declaration for headers
class EfficientViT;
#endif
```

## Expected Impact

### Before Optimization
- 4 includes of torch/torch.h
- Total time: 111.2 seconds
- Each include: 27.8 seconds

### After Optimization (Reducing to 1-2 includes)
- Only include in .cpp files (implementation files)
- Estimated remaining: 2 includes × 27.8s = 55.6 seconds
- **Savings: 55.6 seconds (20% of total build time)**

### If Only in .cpp Files (Ideal)
- All torch includes in implementation files only
- **Maximum savings: 70-90 seconds** (considering transitive includes)

## Implementation Priority

1. **High Priority**: Create torch_fwd.hpp
2. **High Priority**: Refactor torch_helpers.hpp to use forward declarations
3. **Medium Priority**: Refactor efficientvit.hpp 
4. **Low Priority**: Verify all other usages

## Notes

- The Tensor_Data.hpp is already well-optimized using Pimpl
- Focus on torch_helpers.hpp which is likely included transitively many times
- After changes, re-run ClangBuildAnalyzer to measure improvement
- Consider adding a compile-time check to prevent torch/torch.h in headers:

```cpp
// In a commonly included header
#ifdef TORCH_TORCH_H_
  #if !defined(ALLOW_TORCH_IN_HEADER) && !defined(__FILE_IS_CPP__)
    #error "torch/torch.h should not be included in headers. Use torch_fwd.hpp instead."
  #endif
#endif
```

## Trade-offs

**Pros:**
- 50-90 second build time reduction
- Cleaner header dependencies
- Faster incremental builds
- Less memory usage during compilation

**Cons:**
- Slightly more verbose code
- Need to maintain forward declaration header
- Some template code may need to move to implementation files

**Recommendation:** The build time savings far outweigh the minor code complexity increase.
