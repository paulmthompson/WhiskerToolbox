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

**Implementation:**

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

### Proposal 4: Richer SelectionContext Payloads (Priority: Medium)

**Problem addressed:** Limited context (only data key + entities), preventing widgets from making smart decisions.

**Concept:** Extend `SelectionContext` to carry optional metadata about the focused data:

```cpp
struct DataFocusMetadata {
    std::optional<std::size_t> num_rows;
    std::optional<std::size_t> num_columns;
    std::optional<std::vector<std::string>> column_names;
    std::optional<QString> row_type;  // "TimeFrameIndex", "Interval", "Ordinal"
    // Extensible via std::any for future metadata
    std::unordered_map<QString, QVariant> extra;
};

void setDataFocus(SelectedDataKey key, QString data_type,
                  SelectionSource source,
                  DataFocusMetadata metadata = {});
```

**Benefits:**
- Context-aware actions (Proposal 1) can make better applicability decisions without querying DataManager
- Widgets can show richer info in their focus-response UI (e.g., "PCA output: 1000×2, columns: PC1, PC2")
- Scatter widget can auto-suggest column mapping from metadata

**Implementation cost:** Small — add optional metadata to the signal; existing consumers ignore it (backward compatible).

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

## Relationship to Tensor Transforms Roadmap

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
| 4 | Richer SelectionContext Payloads | Medium | Low | Nothing |
| 3 | Workflow Pipeline Descriptors | Low | High | Proposals 1, 2 should land first |

Proposal 1 has the highest impact-to-effort ratio: it eliminates the most manual steps across the widest range of workflows, and its implementation (action registration + context menu/toolbar integration) is well-scoped.
