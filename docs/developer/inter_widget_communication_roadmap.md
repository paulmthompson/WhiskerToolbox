# Inter-Widget Communication — Architecture Analysis & Roadmap

**Date:** 2026-03-21

## Overview

WhiskerToolbox uses a docked-widget architecture where each widget is an independent module with its own `EditorState`. Widgets communicate through shared infrastructure — never via direct pointers to each other. This document analyzes the three existing communication layers, identifies friction points in current workflows, surveys how other applications (particularly Blender) solve similar problems, and proposes improvements.

---

## Current Communication Layers

### Layer 1: DataManager + Observer (Data Change Notifications)

**Mechanism:** Widgets register lambdas via `ObserverData::addObserver()`. When any data object changes or is added to `DataManager`, all registered callbacks fire.

**Characteristics:**
- Fire-and-forget broadcast — no payload, no source tracking
- Widgets must re-query DataManager to discover what changed
- Simple, reliable, zero coupling between widgets
- Non-thread-safe (all on Qt main thread)

**Used for:** Refreshing combo boxes, updating visualizations when data is added/removed/modified.

**Example:**
```
DataTransform_Widget writes PCA output → DataManager::setData("features_pca", tensor)
→ DataManager notifies all observers
→ ScatterPlotPropertiesWidget refreshes key combo boxes
→ TensorInspector refreshes key list
```

**Limitation:** No information about *what* changed. Every observer must re-scan to find differences. Fine for small data counts, but O(widgets × data keys) work per notification.

---

### Layer 2: SelectionContext + DataFocusAware (Passive Awareness)

**Mechanism:** A singleton `SelectionContext` holds the currently focused data key, selected entities, and active editor. Widgets inherit `DataFocusAware` to auto-receive focus changes. Source tracking via `SelectionSource` prevents circular updates.

**Signals:**
| Signal | Payload | Purpose |
|--------|---------|---------|
| `dataFocusChanged` | `SelectedDataKey`, data type, `SelectionSource` | Which data key is active |
| `entitySelectionChanged` | `SelectionSource` | Which entities are selected |
| `selectionChanged` | `SelectionSource` | Legacy catch-all |
| `widgetFocusChanged` | `EditorInstanceId` | Which widget tab is active |
| `timeFrameFocusChanged` | time key, `SelectionSource` | Which TimeFrame is active |

**Used for:** Click on a key in DataManager_Widget → MLCore_Widget auto-selects it as feature tensor. Click on a group in GroupManagementWidget → ScatterPlotWidget highlights those entities.

**Limitation:** Carries only *identity* — "this key is focused" or "these entities are selected." Cannot carry *structured payloads* like "apply this PCA config" or "cluster with these parameters."

---

### Layer 3: OperationContext (Request/Response)

**Mechanism:** A widget (consumer) calls `requestOperation()` specifying a producer widget type and a `DataChannel`. The OperationContext:
1. Opens/focuses the producer widget (via MainWindow wiring)
2. The producer sees a pending operation via `pendingOperationFor()`
3. The producer shows a "Send to [requester]" button
4. User configures the operation in the producer, clicks send
5. Producer calls `deliverResult()` with a type-erased `OperationResult`
6. Consumer receives via `operationDelivered` signal

**Current DataChannels:**
- `DataChannels::TransformPipeline` — JSON pipeline descriptor

**Used for:** ColumnConfigDialog requests a pipeline config from TransformsV2_Widget.

**Strengths:**
- Fully decoupled — consumer knows only the producer *type*, not a specific instance
- Type-erased payload — any data can be passed via `std::any`
- Lifecycle management — operations auto-close on widget destruction or selection change
- One pending operation per producer type — clean state machine

**Limitation:** Currently one-shot, request/response only. No pub/sub. Only one consumer can request from a given producer type at a time. No mechanism for broadcasting a structured payload to *all interested* widgets.

---

## Friction Points in Current Workflows

### Friction 1: Column Builder ↔ TransformsV2 Widget

**Current state (improved from manual JSON copy-paste):**
The `OperationContext` request/response flow is implemented. The ColumnConfigDialog has a "Request from Transforms V2" button that opens the TransformsV2_Widget, which shows a "Send Pipeline to Column Builder" button. The pipeline JSON is delivered automatically.

**Remaining friction:**
- **Discoverability:** Users must know to click "Request from Transforms V2." There's no visual cue that the TransformsV2 widget *can* serve as a pipeline source.
- **One-shot:** After delivery, the operation closes. If the user wants to tweak the pipeline, they must re-request. There's no "keep this connection open for iteration."
- **Unidirectional:** TransformsV2 sends *to* the column builder. There's no mechanism for the column builder to *seed* the TransformsV2 widget with an existing pipeline for editing.

### Friction 2: Dimensionality Reduction → Scatter Plot

**Future workflow (from `tensor_transforms_roadmap.md`):**
1. User runs PCA in DataTransform_Widget (or MLCore Dim Reduction tab)
2. Output lands in DataManager as `"features_pca"` TensorData
3. User wants to visualize in ScatterPlotWidget with X=PC1, Y=PC2

**Steps required today:**
1. Open ScatterPlotWidget (if not open)
2. In ScatterPlot properties, select `"features_pca"` from X key combo
3. Select `"PC1"` from X column combo
4. Select `"features_pca"` from Y key combo
5. Select `"PC2"` from Y column combo

**Friction:** Five manual steps for a common operation. The system knows the output is 2D with named columns, but the user must manually configure each axis.

### Friction 3: Scatter Plot Selection → MLCore Clustering

**Future workflow:**
1. User sees clusters in scatter plot
2. Wants to run K-Means on the visible data
3. Must open MLCore_Widget, select the correct tensor, configure clustering, run

**Friction:** Context switch from visualization to ML widget. The scatter widget already *has* the data and the user can *see* the clusters, but can't directly trigger analysis.

### Friction 4: MLCore Cluster Output → Scatter Recolor

**Current state:** This actually works via EntityGroupManager observer → scatter generation counter → scene dirty. No friction here — it's a success case for Layer 1/2 communication.

### Friction 5: Multi-Widget Coordinated State

**Example:** User is building an analysis: encoder → PCA → scatter → cluster. Each step involves a different widget. The "pipeline" of operations is implicit — there's no way to save/replay this multi-widget workflow.

---

## How Other Applications Handle This

### Blender: Operator System + Properties + Context

Blender's approach is instructive because it faces the same problem: many specialized editors (3D viewport, shader editor, timeline, properties panel) that must coordinate without direct coupling.

**Key patterns:**

1. **Operators (Commands):** Every user action is an Operator with:
   - `poll()` — can this run in the current context?
   - `execute()` / `invoke()` — do the work
   - `draw()` — optional UI for parameters
   - Operators are registered globally and invocable from any context
   - **Applied to WhiskerToolbox:** The Commands system serves this role. An operator like "Run PCA and Open Scatter" could chain multiple widget operations.

2. **Context:** A runtime struct containing the current active object, selected objects, active area/region, mode, etc. **Every operator receives context**, not pointers to specific editors.
   - **Applied to WhiskerToolbox:** `SelectionContext` + `DataManager` together serve this role. The gap is that WhiskerToolbox context doesn't include "current active tensor column" or "current transform configuration" — only data keys and entity selections.

3. **Data API (bpy.data):** All data is globally accessible. Operators read/write through a unified API (Python property system). No editor "owns" data.
   - **Applied to WhiskerToolbox:** `DataManager` serves this role. Widgets read/write through `DataManager` keys. This pattern is already solid.

4. **Notifiers:** Blender has a callback system (`bpy.app.handlers`) for events like frame change, file load, etc. But for fine-grained coordination, Blender relies on **driver expressions** and **dependency graph (depsgraph)** — a DAG that automatically propagates changes.
   - **Applied to WhiskerToolbox:** The Observer system fills this role, but without dependency tracking. There's no DAG that knows "if PCA output changes, scatter plot must rebuild."

5. **Space/Area system:** Each Blender area (editor panel) is generic and can switch its type freely. Areas share a common header with mode selection. **Critically, areas don't know about each other — they only know about the data they display and the operators they can invoke.**
   - **Applied to WhiskerToolbox:** This matches the EditorRegistry + Zone system exactly.

### Key Blender Insight: Rich Context, Dumb Editors

Blender's editors are "dumb views" into a rich shared context. The intelligence is in:
- The **data model** (centralized, dependency-tracked)
- The **operators** (registered globally, context-aware)
- The **properties/settings** (typed, inspectable, animatable)

Editors just render the current state and invoke operators. They don't coordinate with each other directly.

### VS Code: Extension API + Command Palette

VS Code uses a pattern similar to Blender operators:
- **Commands** are globally registered string IDs with typed arguments
- Any extension can register commands; any extension can invoke them
- The **Command Palette** makes *all* commands discoverable
- **When Clauses** (context keys) control command visibility

This is relevant to the "discoverability" problem — users don't know what operations are available for their current context.

---

## Proposed Architecture Improvements

### Proposal 1: Context-Aware Actions (Priority: High)

**Problem addressed:** Friction 2, 3 — too many manual steps for common operations.

**Concept:** Extend `SelectionContext` with a lightweight action discovery system. When a widget sets `dataFocus` on a TensorData with 2 columns named "PC1"/"PC2", other widgets can advertise available actions.

#### Architectural Boundary: Context Actions vs Commands

The existing Commands system (`ICommand`, `CommandFactory`, `CommandContext`) and the proposed Context Actions serve **different layers** and are **compositional**, not competing:

| Dimension | Commands (`ICommand`) | Context Actions (`ContextAction`) |
|-----------|----------------------|----------------------------------|
| **Layer** | Data model — mutates `DataManager` | UI orchestration — opens/configures widgets |
| **Context** | `CommandContext` (DataManager + runtime variables) | `SelectionContext` (focused key, selected entities, data type) |
| **Applicability** | None — no `poll()` or `canExecute()` predicate | `is_applicable(SelectionContext const &)` evaluated dynamically |
| **Serialization** | JSON-serializable via reflect-cpp (`toJson()`) | **Not serializable** — contains `std::function` captures |
| **Undo** | Opt-in via `isUndoable()` / `undo()` | Not undoable (UI state changes are not tracked) |
| **Registration** | Explicit dispatcher via `CommandRegistry` (see [Commands Roadmap Phase 7](DataManager/Commands/roadmap.qmd#phase-7--command-registration-api-explicit-dispatcher)) | Runtime via `SelectionContext::registerAction()` (dynamic) |
| **Invocation** | `executeSequence()` / `executeSingleCommand()` | User clicks context menu / action palette item |
| **Examples** | `CopyByTimeRange`, `SaveData`, `SynthesizeData` | "Visualize in Scatter Plot", "Cluster with K-Means" |

**The key relationship is composition.** A ContextAction's `execute()` body typically:
1. **Performs UI orchestration** — opens/creates a widget, configures its EditorState
2. **Optionally invokes a Command** — for the data-mutation step
3. **Updates SelectionContext** — e.g., `setDataFocus()` on the output

```
┌─────────────────────────────────────────────────────┐
│  ContextAction: "Cluster with K-Means"              │
│                                                     │
│  is_applicable: focused data is TensorData          │
│                                                     │
│  execute():                                         │
│    1. Run clustering (Command or MLCore pipeline)   │  ← data mutation
│    2. Open/focus ScatterPlotWidget                  │  ← UI orchestration
│    3. setDataFocus(output_key)                      │  ← context update
│    4. ScatterPlot auto-colors by cluster groups     │  ← observer cascade
└─────────────────────────────────────────────────────┘
```

**Design rule:** If an operation *only* mutates data, it should be a Command (serializable, undoable, replayable). If it involves UI orchestration — opening widgets, configuring views, navigating — it should be a ContextAction that *may call* Commands internally.

This mirrors the Blender pattern: Blender operators can be "pure data" (like `bpy.ops.mesh.subdivide`) or "UI + data" (like `bpy.ops.screen.area_split`). Both go through the same operator system, but only the data-mutating ones participate in undo. WhiskerToolbox keeps these layers explicitly separate rather than mixing them in one abstraction.

**Future convergence:** If Commands gain a `canExecute(CommandContext)` predicate (tracked in `docs/developer/DataManager/Commands/roadmap.md`), pure-data commands could *also* surface through the ContextAction discovery UI. A thin adapter would wrap a Command as a ContextAction:

```cpp
// Hypothetical adapter — Command exposed as ContextAction
ContextAction fromCommand(CommandInfo const & info, /* ... */) {
    return ContextAction{
        .action_id = "cmd." + info.name,
        .display_name = QString::fromStdString(info.description),
        .is_applicable = [&info](SelectionContext const & ctx) {
            // Check data type compatibility from CommandInfo::supported_data_types
            return std::ranges::contains(info.supported_data_types, ctx.dataFocusType());
        },
        .execute = [name = info.name](SelectionContext const & ctx) {
            // Build CommandContext from SelectionContext + DataManager
            // Invoke executeSingleCommand(name, params, cmd_ctx)
        }
    };
};
```

This would make Commands discoverable in context menus without conflating the two abstractions.

#### Implementation

Add a `ContextAction` system to `SelectionContext`:

```cpp
struct ContextAction {
    QString action_id;          // e.g., "scatter_plot.visualize_2d"
    QString display_name;       // e.g., "Visualize in Scatter Plot"
    QString icon;               // optional icon resource
    EditorTypeId producer_type; // which widget type handles this
    std::function<bool(SelectionContext const &)> is_applicable;
    std::function<void(SelectionContext const &)> execute;
};
```

Widgets register actions at startup:
```cpp
// In ScatterPlotWidgetRegistration:
selection_context->registerAction(ContextAction{
    .action_id = "scatter_plot.visualize_columns",
    .display_name = "Visualize in Scatter Plot",
    .producer_type = EditorTypeId("ScatterPlotWidget"),
    .is_applicable = [](SelectionContext const & ctx) {
        // True if focused data is TensorData with ≥2 columns
        return ctx.dataFocusType() == "TensorData"; 
    },
    .execute = [registry](SelectionContext const & ctx) {
        // Find or create scatter plot, configure X/Y from first two columns
        auto state = registry->findOrCreate("ScatterPlotWidget");
        state->setXSource(ctx.dataFocusKey(), /*col=*/0);
        state->setYSource(ctx.dataFocusKey(), /*col=*/1);
    }
});
```

**Consumer side:** Any widget can query available actions for the current context:
```cpp
auto actions = selection_context->applicableActions();
// Show in context menu, toolbar, or action palette
```

**Benefits:**
- Reduces Friction 2 to one click: "Visualize in Scatter Plot" appears automatically when a 2D tensor is focused
- Reduces Friction 3: "Cluster with K-Means" appears when tensor data is focused
- Discoverable — users see what's possible for their current selection
- No widget-to-widget coupling — actions are registered against *context predicates*, not specific widgets

**Scope:** This is the single highest-leverage improvement. It turns the implicit "the user knows to open the right widget and configure it" into an explicit "the system suggests what you can do next."

---

### Proposal 2: Bidirectional OperationContext ("Edit and Return") (Priority: Medium)

**Problem addressed:** Friction 1 — one-shot delivery with no iteration or seeding.

**Concept:** Extend `OperationContext` to support seeding the producer with initial state, and keeping the connection open for iterative refinement.

**New `OperationRequestOptions` fields:**
```cpp
struct OperationRequestOptions {
    DataChannel channel = DataChannels::TransformPipeline;
    bool close_on_selection_change = true;
    bool close_after_delivery = false;     // NEW default: false for iteration
    bool keep_open_for_iteration = false;  // NEW: don't close after delivery
    std::optional<OperationResult> seed;   // NEW: initial state for producer
};
```

**Workflow:**
1. ColumnConfigDialog has an existing pipeline JSON
2. Clicks "Edit in Transforms V2" → sends `requestOperation()` with `seed = existing_json`
3. TransformsV2_Widget opens with the pipeline pre-loaded
4. User tweaks parameters, clicks "Send"
5. ColumnConfigDialog receives updated JSON
6. Connection stays open (if `keep_open_for_iteration = true`) — user can keep tweaking
7. User closes either widget → operation auto-closes

**Implementation cost:** Small — `OperationContext` already supports all the primitives. Need:
- Add `seed` to `OperationRequestOptions`
- Add `initialSeed()` to `PendingOperation`
- Producer checks `pendingOperationFor()` and reads seed
- UI change: "Edit in Transforms V2" button alongside "Request from Transforms V2"

---

### Proposal 3: Workflow Pipeline Descriptors (Priority: Low)

**Problem addressed:** Friction 5 — multi-widget workflows are implicit and non-reproducible.

**Concept:** Define a JSON-serializable workflow that chains operations across widgets:

```json
{
  "workflow": "Encoder → PCA → Scatter → Cluster",
  "steps": [
    {
      "widget": "DeepLearningWidget",
      "action": "run_inference",
      "params": {"model": "convnext_encoder", "output_key": "encoder_features"}
    },
    {
      "widget": "TransformsV2Widget",
      "action": "execute_transform",
      "params": {
        "transform": "TensorPCA",
        "input_key": "encoder_features",
        "output_key": "pca_features",
        "n_components": 2
      }
    },
    {
      "widget": "ScatterPlotWidget",
      "action": "visualize_columns",
      "params": {"data_key": "pca_features", "x_col": "PC1", "y_col": "PC2"}
    }
  ]
}
```

This is architecturally similar to the existing `CommandSequenceDescriptor` / `TriageSession` pipeline system, but applied to UI workflows rather than data transforms.

**This is deliberately low priority** — Proposals 1 and 2 address the immediate friction. Workflow pipelines are a quality-of-life feature for power users building repeatable analyses.

---

### ~~Proposal 4: Richer SelectionContext Payloads~~ (Removed)

**Status:** Removed. The metadata this proposed (row count, column count, column names, row type) is tensor-specific and would bloat a generic communication layer. ContextActions already capture `std::shared_ptr<DataManager>` and can query data properties directly — one cheap map lookup is always fresh and works for every data type. This follows Blender's pattern: context carries *identity and type*, operators query the *data* for details.

---

## Communication Pattern Decision Matrix

For each type of inter-widget interaction, use the appropriate layer:

| Interaction | Layer | Example |
|-------------|-------|---------|
| "New data exists" | **Layer 1:** DataManager Observer | Transform writes output → scatter refreshes key list |
| "Look at this data" | **Layer 2:** SelectionContext `setDataFocus` | Click key in DataManager_Widget → all widgets update |
| "These entities are selected" | **Layer 2:** SelectionContext `setSelectedEntities` | Select points in scatter → highlight in table |
| "Give me a pipeline config" | **Layer 3:** OperationContext request/response | Column builder ↔ TransformsV2 widget |
| "What can I do with this?" | **Proposal 1:** Context-Aware Actions | Right-click tensor → "Visualize in Scatter" |
| "Edit this and send back" | **Proposal 2:** Bidirectional OperationContext | Edit existing pipeline JSON in TransformsV2 |
| "Run this whole analysis" | **Proposal 3:** Workflow Pipelines | Encoder → PCA → Scatter → Cluster as one-click |

---

## Ownership & Registration Best Practices

This section answers: as operations grow, **who defines each Command and ContextAction**, and where does the code live?

### The Scaling Problem

Today, `CommandFactory` uses a hardcoded if-else chain — every new command requires editing [CommandFactory.cpp](src/Commands/Core/CommandFactory.cpp). The Commands library deliberately has **no dependency** on TransformsV2 or MLCore. This was the right choice for generic CRUD commands (`MoveByTimeRange`, `SaveData`), but it doesn't scale if every PCA variant, clustering algorithm, or dimensionality reduction method becomes a command class in `src/Commands/`.

Meanwhile, TransformsV2 already solved this problem for transforms: RAII static registration lets each transform define itself in its own `.cpp` file, compiled into the library, automatically available at runtime. No central file needs editing.

### Principle: The Provider Defines It

The core principle for both Commands and ContextActions:

> **The library/widget that implements a capability is the one that defines and registers it.**

This follows from dependency direction: if PCA lives in MLCore, then only MLCore (or code that links MLCore) should define the "Run PCA" operation. The Commands library shouldn't grow a dependency on MLCore just to register that command.

### Command Ownership by Category

Commands split into three ownership tiers:

#### Tier 1: Generic CRUD Commands → `src/Commands/`

These are data-model operations that work across all data types and have no domain knowledge:

| Command | Owner | Why |
|---------|-------|-----|
| `CreateData` | `src/Commands/` | Generic DataManager CRUD |
| `DeleteData` | `src/Commands/` | Generic DataManager CRUD |
| `MoveByTimeRange` | `src/Commands/` | Generic entity operations |
| `CopyByTimeRange` | `src/Commands/` | Generic entity operations |
| `SaveData` / `LoadData` | `src/Commands/` | Generic persistence |
| `ForEachKey` | `src/Commands/` | Meta-command (works on any command) |

These belong centrally because they depend only on DataManager and data object types — things Commands already links.

#### Tier 2: Gateway Commands → `src/Commands/`

Thin commands that delegate to an existing execution system. The command provides the serializable/recordable/undoable wrapper; the execution system provides the domain logic:

| Command | Delegates To | Why |
|---------|-------------|-----|
| `RunTransform` | TransformsV2 `ElementRegistry` | One command covers *all* registered transforms |
| `RunPipeline` | MLCore pipeline execution | One command covers all ML workflows |

**Key insight:** A single `RunTransform` command can surface every TransformsV2 transform (PCA, t-SNE, MaskArea, etc.) without knowing about any of them at compile time. It takes a transform name + params as JSON, and the TransformsV2 registry resolves the rest. This is exactly how `SynthesizeData` already works — it delegates to `GeneratorRegistry` without Commands knowing about individual generators.

```cpp
// Hypothetical RunTransform command — lives in src/Commands/
struct RunTransformParams {
    std::string input_key;
    std::string output_key;
    std::string transform_name;     // Resolved by TransformsV2 registry at runtime
    rfl::Generic transform_params;  // Passed through to transform
};
```

The Commands library would need to link TransformsV2 (or accept a function pointer/interface for transform execution in CommandContext). The latter keeps the dependency cleaner:

```cpp
// In CommandContext — optional transform executor injected by the application
std::function<CommandResult(std::string const & transform_name,
                            rfl::Generic const & params,
                            std::shared_ptr<DataManager>)> transform_executor;
```

This way Commands stays ignorant of TransformsV2 internals — the application wires the executor at startup.

#### Tier 3: Domain-Specific Commands → Domain Libraries

If a domain library needs commands that don't fit the gateway pattern (e.g., complex multi-step ML workflows that are a single logical operation), the domain library defines them:

| Command | Owner | Why |
|---------|-------|-----|
| Future: `TrainModel` | `src/MLCore/` | Complex multi-step operation with native MLCore types |
| Future: `RunInference` | `src/DeepLearning/` | Model loading + execution with libtorch types |

These commands implement `ICommand` and are registered via **explicit dispatcher functions** — the same pattern adopted in [Commands Roadmap Phase 7](DataManager/Commands/roadmap.qmd#phase-7--command-registration-api-explicit-dispatcher). Each domain library defines a `register_*_commands()` function that imperatively registers its commands with `CommandRegistry::instance()`:

```cpp
// src/MLCore/commands/register_mlcore_commands.hpp
namespace mlcore {
void register_mlcore_commands();
}

// src/MLCore/commands/register_mlcore_commands.cpp
#include <Commands/Core/CommandRegistry.hpp>
#include "TrainModelCommand.hpp"

namespace mlcore {
void register_mlcore_commands() {
    commands::registerTypedCommand<TrainModelCommand, TrainModelParams>(
        commands::CommandRegistry::instance(),
        "TrainModel",
        {.name = "TrainModel",
         .description = "Train an ML model",
         .category = "ml",
         .supports_undo = false});
}
}
```

The central dispatcher in the executable calls all registration functions at startup:

```cpp
// src/WhiskerToolbox/register_all_commands.cpp
#include <Commands/Core/register_core_commands.hpp>
#include <MLCore/commands/register_mlcore_commands.hpp>

void register_all_commands() {
    commands::register_core_commands();
    mlcore::register_mlcore_commands();
}
```

This avoids `--whole-archive` linker flags and produces loud linker errors on misconfiguration (vs. silent registration absence with the RAII pattern).

### ContextAction Ownership: Widgets Define What They Provide

ContextActions belong to the **widget that provides the capability**, registered during widget type registration. This follows naturally from the existing `EditorRegistry` pattern.

**Why the widget?** Because only the widget knows:
- What data shapes it can consume (scatter needs ≥2 columns; heatmap needs a matrix)
- How to configure itself for the incoming data (scatter maps columns to axes)
- What UI it presents (scatter vs. line plot vs. table)

**Where in the code?** In the widget's registration file, which already captures EditorRegistry, DataManager, and GroupManager. Adding SelectionContext to the registration captures provides the hook:

```cpp
// In ScatterPlotWidgetRegistration.cpp
void registerScatterPlotWidget(EditorRegistry * registry,
                                std::shared_ptr<DataManager> dm,
                                GroupManager * gm,
                                SelectionContext * selection_ctx)  // NEW parameter
{
    // Existing widget registration
    registry->registerType({ .type_id = "ScatterPlotWidget", /* ... */ });

    // NEW: register what this widget can do for the user
    selection_ctx->registerAction(ContextAction{
        .action_id = "scatter_plot.visualize_2d_tensor",
        .display_name = "Visualize in Scatter Plot",
        .producer_type = EditorTypeId("ScatterPlotWidget"),
        .is_applicable = [dm](SelectionContext const & ctx) {
            // Only for TensorData with ≥2 columns
            if (ctx.dataFocusType() != "TensorData") return false;
            auto tensor = dm->getData<TensorData>(ctx.dataFocusKey());
            return tensor && tensor->numColumns() >= 2;
        },
        .execute = [registry, dm](SelectionContext const & ctx) {
            auto [state, _] = registry->findOrCreate("ScatterPlotWidget");
            auto scatter_state = std::dynamic_pointer_cast<ScatterPlotState>(state);
            auto tensor = dm->getData<TensorData>(ctx.dataFocusKey());
            auto cols = tensor->columnNames();
            scatter_state->setXSource({ctx.dataFocusKey(), cols[0]});
            scatter_state->setYSource({ctx.dataFocusKey(), cols[1]});
        }
    });
}
```

Similarly, MLCore_Widget registers ML-oriented actions:

```cpp
// In MLCoreWidgetRegistration.cpp
selection_ctx->registerAction(ContextAction{
    .action_id = "mlcore.cluster_tensor",
    .display_name = "Cluster with K-Means",
    .producer_type = EditorTypeId("MLCoreWidget"),
    .is_applicable = [dm](SelectionContext const & ctx) {
        return ctx.dataFocusType() == "TensorData";
    },
    .execute = [registry](SelectionContext const & ctx) {
        auto [state, _] = registry->findOrCreate("MLCoreWidget");
        auto ml_state = std::dynamic_pointer_cast<MLCoreWidgetState>(state);
        ml_state->setFeatureTensorKey(ctx.dataFocusKey());
        ml_state->setActiveTab(MLCoreTab::Clustering);
    }
});
```

### ContextActions That Compose Commands

A ContextAction that both mutates data *and* orchestrates UI combines both systems:

```
┌──────────────────────────────────────────────────────────────────┐
│  ContextAction: "Run PCA and Visualize" (registered by          │
│                  DataTransform_Widget or a future Analysis_Widget)│
│                                                                  │
│  execute():                                                      │
│    1. executeSingleCommand("RunTransform", {                     │
│         transform: "TensorPCA", input: focused_key,              │
│         output: focused_key + "_pca", n_components: 2            │  ← Command (data)
│       })                                                         │
│    2. Open/configure ScatterPlotWidget for output                │  ← UI orchestration
│    3. setDataFocus(output_key)                                   │  ← Context update
└──────────────────────────────────────────────────────────────────┘
```

The Command is recorded in the CommandRecorder (serializable, replayable, auditable). The UI orchestration is ephemeral — it's not recorded because it doesn't change data.

### Summary Table

| What | Defined By | Registered Where | Registration Mechanism |
|------|-----------|-----------------|----------------------|
| Generic CRUD commands | `src/Commands/` | `CommandRegistry` (via `register_core_commands()`) | Explicit dispatcher ([Phase 7](DataManager/Commands/roadmap.qmd#phase-7--command-registration-api-explicit-dispatcher)) |
| Gateway commands (`RunTransform`) | `src/Commands/` | `CommandRegistry` (via `register_core_commands()`) | Delegates to domain registry via injected executor |
| Domain-specific commands | Domain library (`src/MLCore/`, etc.) | `CommandRegistry` (via `register_*_commands()`) | Explicit dispatcher — one line in `register_all_commands.cpp` |
| Transforms (element-level) | `src/TransformsV2/` | ElementRegistry singleton | RAII static `RegisterTransform<>` |
| ContextActions | Provider widget | SelectionContext | Widget registration function |

### Migration Path

The recommended migration sequence aligns with [Commands Roadmap Phase 7](DataManager/Commands/roadmap.qmd#phase-7--command-registration-api-explicit-dispatcher):

1. **Phase 7 (Commands):** Replace the hardcoded if-else chain in `CommandFactory` with `CommandRegistry` singleton + `register_core_commands()`. All existing commands self-register via explicit dispatcher functions. No `--whole-archive` needed.
2. **When adding `RunTransform`:** Add `transform_executor` callback to `CommandContext`. This avoids a hard dependency from Commands → TransformsV2 while enabling transform invocation through the command system.
3. **When domain libraries contribute commands:** Each domain library defines a `register_*_commands()` function. The central `register_all_commands.cpp` in the executable gains one line per library. Linker errors catch misconfigurations.
4. **ContextActions from day one:** Use the widget registration pattern described above. No migration needed — it's greenfield.

The `tensor_transforms_roadmap.md` phases depend on the communication improvements described here:

| Tensor Roadmap Phase | Communication Dependency |
|-----------------------|--------------------------|
| Phase 1 (PCA Foundation) | Layer 1 only — output lands in DataManager, observer notifies scatter |
| Phase 2 (Scatter Selection → Groups) | Layer 2 — EntityGroupManager notifications propagate via observers |
| Phase 3 (Tapkee) | No new communication needs |
| Phase 4 (MLCore Dim Reduction + Auto-Focus) | Layer 2 — `setDataFocus()` after transform; **Proposal 1** for "Visualize in Scatter" action |
| Phase 5 (Clustering ↔ Scatter) | **Proposal 1** — context-aware "Cluster Points" action in scatter context menu |
| Phase 7 (Advanced) | **Proposal 3** — reproducing encoder→PCA→scatter→cluster workflow |

**Recommended sequencing:** Implement Proposal 1 (Context-Aware Actions) alongside or shortly after Tensor Roadmap Phase 2, as it directly enables Phases 4-5.

---

## Relationship to Tensor Refactor Roadmap

The `tensor_roadmap.md` item #1 (interval-row support for non-analog types) has implications for inter-widget communication:

Once `MaskData`, `LineData`, and `PointData` are supported as interval-row sources for TensorData columns, the entity preservation story must extend beyond `TimeFrameIndex` rows. Scatter plot selections on interval-row TensorData will need to map back to the originating intervals and their associated entities. This is tracked in `tensor_transforms_roadmap.md` Phase 2, item 3, under "Future extension."

---

## Implementation Priority Summary

| # | Proposal | Priority | Complexity | Blocked By |
|---|----------|----------|------------|------------|
| 1 | Context-Aware Actions | **High** | Medium | Nothing — can start immediately |
| 2 | Bidirectional OperationContext | Medium | Low | Nothing |
| ~~4~~ | ~~Richer SelectionContext Payloads~~ | ~~Medium~~ | ~~Low~~ | Removed — ContextActions query DataManager directly |
| 3 | Workflow Pipeline Descriptors | Low | High | Proposals 1, 2 should land first |

Proposal 1 has the highest impact-to-effort ratio: it eliminates the most manual steps across the widest range of workflows, and its implementation (action registration + context menu/toolbar integration) is well-scoped.

---

## Cross-Roadmap Implementation Sequence

The three roadmaps — [Commands](DataManager/Commands/roadmap.qmd), [Tensor Transforms](tensor_transforms_roadmap.md), and this document — are interdependent. This section defines a concrete implementation sequence that resolves dependencies and progressively eliminates friction.

### Dependency Map

```
Commands Phase 7 (CommandRegistry + explicit dispatcher)
    │
    │ enables
    ▼
Tensor Phase 1 (PCA via mlpack + TransformsV2 wrapper)  ←── no command dependency
    │                                                         (pure Transform, not Command)
    │ enables scatter visualization of reduced features
    ▼
Tensor Phase 2 (Scatter Selection → Entity Groups)      ←── no command dependency
    │                                                         (direct EntityGroupManager API)
    │ enables interactive grouping
    ▼
Inter-Widget Proposal 1 (ContextAction framework)        ←── no hard dependency on above,
    │                                                         but most useful after Phases 1-2
    │                                                         provide the data and interactions
    │                                                         that ContextActions orchestrate
    ▼
Tensor Phase 4 (MLCore Dim Reduction Tab + Auto-Focus)   ←── uses Proposal 1
    │   ContextAction: "Visualize in Scatter"
    ▼
Tensor Phase 5 (Clustering ↔ Scatter)                    ←── uses Proposal 1
    │   ContextAction: "Cluster with K-Means"
    ▼
Inter-Widget Proposal 2 (Bidirectional OperationContext)  ←── independent, can be done earlier
    │
    ▼
Commands: RunTransform gateway command                    ←── depends on Phase 7 + Tensor Phase 1
    │   Makes transforms serializable/replayable
    ▼
Inter-Widget Proposal 3 (Workflow Pipelines)              ←── depends on everything above
```

### Recommended Step Sequence

The steps below are ordered to maximize user-visible value at each milestone while respecting dependencies. Steps within the same tier can be done in any order.

#### Tier 1: Foundation (No Cross-Dependencies)

These are independent and can proceed in parallel. They establish the infrastructure that later tiers build on.

| Step | Source Roadmap | Description | Delivers |
|------|---------------|-------------|----------|
| ✅ **1a** | Commands Phase 7, Steps 7.1–7.4 | Create `CommandRegistry`, `registerTypedCommand<>`, `register_core_commands()`, `register_all_commands.cpp`. Remove if-else chain. | Extensible command registration; no `--whole-archive` for commands |
| ✅ **1b** | Tensor Phase 1 | `MLDimReductionOperation` base class + `PCAOperation` in MLCore (mlpack backend) + `TensorPCA` TransformsV2 container wrapper with `RowDescriptor` preservation | PCA through DataTransform_Widget; output visible in scatter |

**Milestone 1:** ✅ User can run PCA on encoder features via DataTransform_Widget, open ScatterPlotWidget, manually select the PCA tensor + columns, and see points.

#### Tier 2: Interactive Selection

| Step | Source Roadmap | Description | Delivers |
|------|---------------|-------------|----------|
| ✅ **2a** | Tensor Phase 2 | "Create Group from Selection" context menu in ScatterPlotWidget. Maps selected `TimeFrameIndex` points → `EntityId` → `EntityGroupManager::createGroup()` | Visual cluster selection → entity groups |
| **2b** | Tensor Phase 1 (verification) | Auto-focus output: `DataTransform_Widget` calls `setDataFocus(output_key)` after executing a transform | ScatterPlot auto-refreshes to show new tensor |

**Milestone 2:** End-to-end workflow works: encoder → PCA → scatter → select cluster → create group. All manual, but functional.

#### Tier 3: ContextAction Framework

| Step | Source Roadmap | Description | Delivers |
|------|---------------|-------------|----------|
| **3a** ✅ | Inter-Widget Proposal 1 | `ContextAction` struct, `SelectionContext::registerAction()`, `applicableActions()` query, context menu integration in DataManager_Widget | Action discovery system |
| **3b** ✅ | Inter-Widget Proposal 1 | Register first ContextActions: ScatterPlotWidget → `"scatter_plot.visualize_2d_tensor"`, MLCore_Widget → `"mlcore.cluster_tensor"`. `EditorRegistry::openEditor()` callback for ContextAction editor creation. | "Visualize in Scatter" and "Cluster with K-Means" appear in context menus |

**Milestone 3:** ✅ Friction 2 (dim reduction → scatter) reduced from 5 manual steps to 1 click. Friction 3 (scatter → clustering) reduced from 3 steps to 1 click.

#### Tier 4: Integrated UI

| Step | Source Roadmap | Description | Delivers |
|------|---------------|-------------|----------|
| **4a** | Tensor Phase 4 | MLCore_Widget "Dimensionality Reduction" tab with `DimensionalityReductionPipeline`, algorithm selection, AutoParamWidget params | Dedicated dim reduction UI (alternative to DataTransform_Widget) |
| **4b** | Tensor Phase 5 | MLCore_Widget ContextAction for clustering from scatter context menu; selective clustering on sub-selection | Tight scatter ↔ clustering loop |
| **4c** | Inter-Widget Proposal 2 | Bidirectional OperationContext: `seed` + `keep_open_for_iteration` options. "Edit in Transforms V2" button in ColumnConfigDialog | Friction 1 (Column Builder ↔ TransformsV2) fully resolved |

**Milestone 4:** The full encoder → PCA → scatter → cluster loop is smooth and discoverable.

#### Tier 5: Commands Integration & Reproducibility

| Step | Source Roadmap | Description | Delivers |
|------|---------------|-------------|----------|
| **5a** | Commands (gateway) | `RunTransform` gateway command: takes transform name + params JSON, delegates to `ElementRegistry` via `transform_executor` in `CommandContext` | Transforms are serializable/replayable commands |
| **5b** | Commands Phase 3.5 | Wire `RunTransform` into `CommandRecorder`. User runs PCA via widget → Command Log shows `RunTransform` entry → exportable JSON | Transform operations appear in command log |
| **5c** | Tensor Phase 3 | Add tapkee dependency; `TSNEOperation`, `IsomapOperation` in MLCore; TransformsV2 wrappers | Non-linear dim reduction |

**Milestone 5:** Analysis workflows are recorded and reproducible. Non-linear methods available.

#### Tier 6: Advanced (Long-Term)

| Step | Source Roadmap | Description | Delivers |
|------|---------------|-------------|----------|
| **6a** | Inter-Widget Proposal 3 | Workflow Pipeline Descriptors: JSON-serializable multi-widget workflows | One-click replay of encoder → PCA → scatter → cluster |
| **6b** | Tensor Phase 7 | Fit/project workflow, scree plot, 3D scatter | Production-quality ML tooling |
| **6c** | Commands Phase 5 | Undo/Redo with `CommandStack` | Undoable transform + grouping operations |

### Friction Resolution Summary

| Friction | Resolved By | Tier |
|----------|-------------|------|
| **F1:** Column Builder ↔ TransformsV2 (discoverability, one-shot, unidirectional) | Tier 4c: Bidirectional OperationContext | 4 |
| **F2:** Dim Reduction → Scatter (5 manual steps) | Tier 2b (auto-focus) + Tier 3b (`"Visualize in Scatter"` ContextAction) | 2–3 |
| **F3:** Scatter → MLCore Clustering (context switch) | Tier 3b (`"Cluster with K-Means"` ContextAction) | 3 |
| **F4:** Cluster → Scatter Recolor | Already works (Layer 1/2) | 0 |
| **F5:** Multi-widget workflow not reproducible | Tier 5a-b (RunTransform command recording) + Tier 6a (Workflow Pipelines) | 5–6 |
