# DataManager → DataObjects Reorganization Plan

## Motivation

The `src/DataManager/` folder currently contains several independent static libraries (PointData, LineData, MaskData, etc.) that exist alongside the DataManager itself. This is a historical artifact — DataManager used to own the data types directly, but each has since been extracted into its own static library. The current layout is confusing because:

1. **Data type libraries are nested inside DataManager** even though they have no dependency on it.
2. **`DataManagerFwd.hpp` / `DataManagerTypes.hpp` coupling** — consumers that only need `DM_DataType` (an enum) or forward declarations are forced to link the entire DataManager library with all its heavy transitive dependencies (CoreMath, Whisker-Analysis, etc.).
3. **`IODataType` is a parallel enum** to `DM_DataType`, maintained only because the IO system couldn't include `DM_DataType` without pulling in DataManager. This duplication creates maintenance overhead and conversion boilerplate.

## Proposed Structure

```
src/
├── DataObjects/
│   ├── CMakeLists.txt             # umbrella, adds all subdirectories
│   ├── DataTypeEnum/              # INTERFACE lib — just the enum + forward declarations
│   │   ├── CMakeLists.txt
│   │   └── DM_DataType.hpp
│   ├── DataTypeVariant/           # INTERFACE lib — variant typedef, links all data type libs
│   │   ├── CMakeLists.txt
│   │   └── DataTypeVariant.hpp
│   ├── TypeTraits/                # INTERFACE lib — DataTypeTraits.hpp
│   │   ├── CMakeLists.txt
│   │   └── DataTypeTraits.hpp
│   ├── AnalogTimeSeries/          # static lib (moved from DataManager/)
│   ├── DigitalTimeSeries/         # static lib (moved from DataManager/)
│   ├── Lines/                     # static lib (moved from DataManager/)
│   ├── Masks/                     # static lib (moved from DataManager/)
│   ├── Points/                    # static lib (moved from DataManager/)
│   ├── Media/                     # static lib (moved from DataManager/)
│   └── Tensors/                   # static lib (moved from DataManager/)
├── IO/                            # moved from DataManager/IO/
│   └── (links DataTypeEnum instead of having IODataType)
├── DataManager/                   # slimmed down — orchestration + transforms only
│   ├── DataManager.hpp/.cpp
│   ├── DataManagerSnapshot.*
│   ├── Commands/
│   ├── Lineage/
│   ├── transforms/
│   └── utils/
└── ...
```

## Key Design Decisions

### 1. DataTypeTraits → Header-only INTERFACE library

`DataTypeTraits.hpp` is a single header with zero non-standard includes (only `<concepts>` and `<type_traits>`). The correct CMake pattern for this is an **INTERFACE library**:

```cmake
# src/DataObjects/TypeTraits/CMakeLists.txt
add_library(DataTypeTraits INTERFACE)
target_include_directories(DataTypeTraits INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
```

Each data type library that defines traits adds:
```cmake
target_link_libraries(MyDataLib PUBLIC DataTypeTraits)
```

This is the standard CMake idiom for a shared header-only dependency among sibling libraries. It costs nothing at link time and gives proper include path propagation.

### 2. Split DataManagerFwd / DataManagerTypes into two layers

The current situation:

| File | Contents | Problem |
|------|----------|---------|
| `DataManagerFwd.hpp` | Forward declarations + `DM_DataType` enum | Lives inside DataManager; no way to link it independently |
| `DataManagerTypes.hpp` | `DataTypeVariant` typedef + `IODataType` bridge | Includes `DataManagerFwd.hpp` + `IOTypes.hpp` + `<variant>` |

The fix is two independent INTERFACE libraries at different weight classes:

#### Layer 1: `DataTypeEnum` (zero dependencies)

```
src/DataObjects/DataTypeEnum/
├── CMakeLists.txt
└── DM_DataType.hpp    # enum + forward declarations
```

```cmake
# CMakeLists.txt
add_library(DataTypeEnum INTERFACE)
target_include_directories(DataTypeEnum INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
```

```cpp
// DM_DataType.hpp — contents of current DataManagerFwd.hpp
class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;
class LineData;
class MaskData;
class MediaData;
class PointData;
class RaggedAnalogTimeSeries;
class TensorData;

enum class DM_DataType {
    Video, Images, Points, Mask, Line,
    Analog, RaggedAnalog,
    DigitalEvent, DigitalInterval,
    Tensor, Time, Unknown
};
```

**Consumers:** IO system, UI widgets that just need the enum, anything that currently includes `DataManagerFwd.hpp`.

#### Layer 2: `DataTypeVariant` (links all data type libraries)

```
src/DataObjects/DataTypeVariant/
├── CMakeLists.txt
└── DataTypeVariant.hpp    # variant typedef, DataInfo, DataGroup
```

```cmake
# CMakeLists.txt
add_library(DataTypeVariant INTERFACE)
target_link_libraries(DataTypeVariant INTERFACE
    DataTypeEnum
    AnalogTimeSeries DigitalTimeSeries LineData MaskData
    PointData MediaData TensorData
)
target_include_directories(DataTypeVariant INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
```

```cpp
// DataTypeVariant.hpp
#include "DM_DataType.hpp"          // from DataTypeEnum
#include "AnalogTimeSeries.hpp"     // actual type definitions needed for variant
#include "DigitalEventSeries.hpp"
// ... etc.

#include <memory>
#include <variant>

using DataTypeVariant = std::variant<
    std::shared_ptr<MediaData>,
    std::shared_ptr<PointData>,
    std::shared_ptr<LineData>,
    std::shared_ptr<MaskData>,
    std::shared_ptr<AnalogTimeSeries>,
    std::shared_ptr<RaggedAnalogTimeSeries>,
    std::shared_ptr<DigitalEventSeries>,
    std::shared_ptr<DigitalIntervalSeries>,
    std::shared_ptr<TensorData>>;

struct DataInfo {
    std::string key;
    std::string data_class;
    std::string color;
};

struct DataGroup {
    std::string groupName;
    std::vector<std::string> dataKeys;
};
```

**Consumers:** TransformsV2, DataManager, DataTransform_Widget — anything that needs the actual variant type.

#### Resulting Linking Hierarchy

```
DataTypeEnum              ← enum only, zero deps
    ↑
DataTypeVariant           ← variant typedef + all data type libs (no transforms, no CoreMath)
    ↑
DataManager               ← computation, transforms, snapshots, observers
```

Consumers pick the lightest layer they actually need. This eliminates the "I need to link DataManager just for an enum" problem.

### 3. Eliminate `IODataType` — use `DM_DataType` everywhere

With `DataTypeEnum` as a zero-dependency INTERFACE library, the IO system can link it directly. `IODataType` becomes completely redundant.

**What gets removed:**
- `IO/core/IOTypes.hpp` (the `IODataType` enum)
- `toIODataType()` / `fromIODataType()` conversion functions in `DataManagerTypes.hpp`
- All `static_assert` sync checks between the two enums

**Migration:** Mechanical find-and-replace of `IODataType::X` → `DM_DataType::X` in IO code and tests. The only behavioral difference is `RaggedAnalog` — `IODataType` didn't have it. At the IO API level, `RaggedAnalog` can be handled as `Analog` (which the conversion functions already did).

### 4. Move IO to `src/IO/`

`src/DataManager/IO/` is already a self-contained plugin system with its own registry. Lifting it to `src/IO/` makes the independence explicit. It links `DataTypeEnum` instead of maintaining a parallel enum.

## Current vs. Proposed Dependencies

### Current (problematic)
```
UI Widget needs DM_DataType
    → includes DataManagerFwd.hpp
    → path resolves inside DataManager
    → links DataManager (PUBLIC)
    → transitively pulls in CoreMath, Whisker-Analysis, StateEstimation, etc.
```

### Proposed (clean)
```
UI Widget needs DM_DataType
    → links DataTypeEnum (INTERFACE, zero deps)
    → includes DM_DataType.hpp
    → done
```

## Data Type Library Dependencies (unchanged)

Each data type library retains its current dependencies — they are already minimal:

| Library | Public Dependencies |
|---------|-------------------|
| AnalogTimeSeries | TimeFrame, Entity, Observer, nlohmann_json |
| DigitalTimeSeries | TimeFrame, Entity, Observer, nlohmann_json |
| LineData | TimeFrame, Entity, Observer, CoreGeometry |
| MaskData | TimeFrame, Entity, Observer, CoreGeometry |
| PointData | TimeFrame, Entity, Observer, CoreGeometry |
| MediaData | TimeFrame, Entity, Observer |
| TensorData | TimeFrame, Entity, Observer, Armadillo |

## Migration Checklist

- [ ] Create `src/DataObjects/` directory structure
- [ ] Create `DataTypeEnum` INTERFACE library (extract from `DataManagerFwd.hpp`)
- [ ] Create `DataTypeVariant` INTERFACE library (extract from `DataManagerTypes.hpp`)
- [ ] Create `DataTypeTraits` INTERFACE library (move from `DataManager/TypeTraits/`)
- [ ] Move data type libraries: AnalogTimeSeries, DigitalTimeSeries, Lines, Masks, Points, Media, Tensors
- [ ] Move `DataManager/IO/` → `src/IO/`
- [ ] Replace `IODataType` with `DM_DataType` in all IO code
- [ ] Remove `IOTypes.hpp`, `toIODataType()`, `fromIODataType()`
- [ ] Update all `#include` paths
- [ ] Update all `CMakeLists.txt` `target_link_libraries` to use new library names
- [ ] Update `src/CMakeLists.txt` `add_subdirectory` calls
- [ ] Fix tests include paths and library links
- [ ] Build and test
- [ ] Update developer documentation
- [ ] Update `.github/copilot-instructions.md` architecture section
