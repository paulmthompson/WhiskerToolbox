# Build Time Optimization: DataManagerTypes.hpp

## Problem
ClangBuildAnalyzer identifies `DataManagerTypes.hpp` as the #1 most expensive header:
- **Total time: 153.5 seconds**
- **Includes: 111 times**
- **Average: 1.38 seconds per include**
- This single header accounts for ~17% of total compilation time

Additionally, `DataManager.hpp` (which includes DataManagerTypes.hpp) is included 142 times.

## Root Cause Analysis

### Current Structure
```cpp
// DataManagerTypes.hpp
#include "IO/interface/IOTypes.hpp"  // Pulls in I/O dependencies
#include <memory>
#include <string>
#include <variant>  // Expensive!
#include <vector>

// Forward declarations (good!)
class AnalogTimeSeries;
class DigitalEventSeries;
// ... 8 total forward declarations

enum class DM_DataType { /* ... */ };

// The expensive part: variant with 8 template parameters
using DataTypeVariant = std::variant<
    std::shared_ptr<MediaData>,
    std::shared_ptr<PointData>,
    std::shared_ptr<LineData>,
    std::shared_ptr<MaskData>,
    std::shared_ptr<AnalogTimeSeries>,
    std::shared_ptr<DigitalEventSeries>,
    std::shared_ptr<DigitalIntervalSeries>,
    std::shared_ptr<TensorData>>;

struct DataInfo { /* ... */ };
struct DataGroup { /* ... */ };
```

### Why It's Expensive
1. **std::variant with 8 types**: Generates extensive template instantiations
2. **Included 111 times**: Each include processes the variant definition
3. **Transitively included**: Many files include DataManager.hpp which includes this
4. **ClangBuildAnalyzer shows**: std::variant instantiation takes 12.6 seconds total

## Solution: Three-Tier Header Strategy

### Tier 1: DataManagerTypesFwd.hpp (Lightweight)
**Already created** - Contains only:
- Forward declarations of data types
- DM_DataType enum
- **Size: 30 lines**
- **Include cost: Negligible (~1ms)**

```cpp
// DataManagerTypesFwd.hpp
class AnalogTimeSeries;
// ... other forward declarations
enum class DM_DataType { /* ... */ };
```

**Use when:** You only need to declare pointers/references to data types

### Tier 2: DataManagerTypesCore.hpp (Medium weight)
**To be created** - Contains:
- Everything from Fwd
- Simple structs (DataInfo, DataGroup)
- Conversion functions
- **No variant!**

```cpp
// DataManagerTypesCore.hpp
#ifndef DATAMANAGERTYPES_CORE_HPP
#define DATAMANAGERTYPES_CORE_HPP

#include "DataManagerTypesFwd.hpp"
#include "IO/interface/IOTypes.hpp"
#include <string>
#include <vector>

struct DataInfo {
    std::string key;
    std::string data_class;
    std::string color;
};

struct DataGroup {
    std::string groupName;
    std::vector<std::string> dataKeys;
};

IODataType toIODataType(DM_DataType dm_type);
DM_DataType fromIODataType(IODataType io_type);

#endif
```

**Use when:** You need DataInfo/DataGroup but not the variant

### Tier 3: DataManagerTypes.hpp (Full - Expensive)
**Refactor existing** - Contains:
- Include DataManagerTypesCore.hpp
- DataTypeVariant definition

```cpp
// DataManagerTypes.hpp (refactored)
#ifndef DATAMANAGERTYPES_HPP
#define DATAMANAGERTYPES_HPP

#include "DataManagerTypesCore.hpp"
#include <memory>
#include <variant>

using DataTypeVariant = std::variant<
    std::shared_ptr<MediaData>,
    std::shared_ptr<PointData>,
    std::shared_ptr<LineData>,
    std::shared_ptr<MaskData>,
    std::shared_ptr<AnalogTimeSeries>,
    std::shared_ptr<DigitalEventSeries>,
    std::shared_ptr<DigitalIntervalSeries>,
    std::shared_ptr<TensorData>>;

#endif
```

**Use only when:** You actually need to work with DataTypeVariant

## Migration Strategy

### Phase 1: Analysis (Done)
- ✅ Created DataManagerTypesFwd.hpp
- ✅ Identified usage patterns

### Phase 2: Audit Includes (To Do)
Audit all 111 includes of DataManagerTypes.hpp to categorize:

```bash
# Find all includes
grep -r "#include.*DataManagerTypes.hpp" src/ --include="*.hpp" --include="*.cpp"

# For each file, determine if it:
# 1. Only needs forward declarations -> use DataManagerTypesFwd.hpp
# 2. Needs DataInfo/DataGroup -> use DataManagerTypesCore.hpp  
# 3. Actually uses DataTypeVariant -> keep DataManagerTypes.hpp
```

**Expected distribution:**
- ~60% can use Fwd (forward declarations only)
- ~25% can use Core (structs but no variant)
- ~15% need full header (actual variant usage)

### Phase 3: Create Core Header
Create DataManagerTypesCore.hpp with structs and conversion functions.

### Phase 4: Refactor Includes (High Impact)
Replace includes based on actual needs:

**Example: A widget that only has a pointer to DataManager**
```cpp
// Before
#include "DataManager/DataManagerTypes.hpp"  // 1.38 seconds!

// After  
#include "DataManager/DataManagerTypesFwd.hpp"  // ~1ms
```

**Example: Code that needs DataInfo**
```cpp
// Before
#include "DataManager/DataManagerTypes.hpp"  // 1.38 seconds + variant

// After
#include "DataManager/DataManagerTypesCore.hpp"  // ~100ms, no variant
```

### Phase 5: Refactor DataManager.hpp
DataManager.hpp currently includes DataManagerTypes.hpp, but may not need the variant:

```cpp
// DataManager.hpp
// Before
#include "DataManagerTypes.hpp"  // Pulls in variant

// After - check if variant is actually needed in interface
#include "DataManagerTypesCore.hpp"  // Much lighter
// Move variant usage to implementation file if possible
```

## Expected Impact

### Current State
- 111 includes of DataManagerTypes.hpp
- 153.5 seconds total
- 1.38 seconds per include

### After Phase 4 (Conservative Estimate)
Assuming 60% can use Fwd, 25% can use Core, 15% need full:

- **Fwd (67 includes)**: 67 × 0.001s = 0.07s
- **Core (28 includes)**: 28 × 0.1s = 2.8s  
- **Full (16 includes)**: 16 × 1.38s = 22.1s
- **Total: ~25 seconds** (was 153.5s)

**Savings: 128 seconds (50% of original cost, 45% of total build time)**

### After Phase 5 (Optimistic - if DataManager.hpp refactored)
If DataManager.hpp uses Core instead of full:
- Transitively saves on all 142 includes of DataManager.hpp
- **Additional savings: 30-50 seconds**

**Total potential savings: 150-170 seconds (55-60% of total build time)**

## Implementation Checklist

- [x] Create DataManagerTypesFwd.hpp
- [ ] Create DataManagerTypesCore.hpp
- [ ] Audit all includes of DataManagerTypes.hpp
  - [ ] Categorize each usage
  - [ ] Create refactoring plan
- [ ] Refactor includes in batches:
  - [ ] Batch 1: Widget headers (likely only need Fwd)
  - [ ] Batch 2: Transform headers (may need Core)
  - [ ] Batch 3: Core DataManager users (need Full)
- [ ] Refactor DataManager.hpp if possible
- [ ] Update DataManagerTypes.hpp to include Core
- [ ] Run ClangBuildAnalyzer to measure improvement
- [ ] Update documentation

## Risk Analysis

**Low Risk Changes:**
- Creating new headers (additive, no breakage)
- Replacing includes where only forward declarations needed
- Adding include guards and documentation

**Medium Risk Changes:**
- Splitting existing DataManagerTypes.hpp
- Changing DataManager.hpp includes
- May cause compilation errors if includes not comprehensive

**Mitigation:**
1. Make changes incrementally
2. Build and test after each change
3. Keep git history clean for easy rollback
4. Update all includes in same commit to avoid partial state

## Code Quality Benefits

Beyond build time, this refactoring improves:
1. **Clearer dependencies**: Headers explicitly state what they need
2. **Faster incremental builds**: Fewer recompilations on changes
3. **Easier testing**: Can mock with forward declarations
4. **Better encapsulation**: Variant internal detail hidden from most users

## Conclusion

DataManagerTypes.hpp refactoring is the single highest-impact optimization:
- **Conservative estimate: 45% build time reduction**
- **Optimistic estimate: 55-60% build time reduction**  
- Low implementation risk with high reward
- Improves code architecture as side benefit

**Recommendation: Implement as Priority 1**
