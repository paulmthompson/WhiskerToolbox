# Inter-Widget Communication — Architecture Analysis & Roadmap

**Last updated:** 2025-07-22

## Overview

WhiskerToolbox uses a docked-widget architecture where each widget is an independent module with its own `EditorState`. Widgets communicate through shared infrastructure — never via direct pointers to each other. This document analyzes the three existing communication layers, identifies friction points in current workflows, surveys how other applications solve similar problems, and proposes improvements.

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

**Mechanism:** A widget (consumer) calls `requestOperation()` specifying a producer widget type and a `DataChannel`. The OperationContext opens/focuses the producer; the producer sees a pending operation, shows a "Send to [requester]" button; user configures and clicks send; producer calls `deliverResult()`.

**Current DataChannels:** `DataChannels::TransformPipeline` — JSON pipeline descriptor.

**Used for:** ColumnConfigDialog requests a pipeline config from TransformsV2_Widget.

**Strengths:** Fully decoupled, type-erased payload, lifecycle management, clean state machine.

**Limitation:** Currently one-shot, request/response only. No pub/sub. Only one consumer can request from a given producer type at a time. No mechanism for broadcasting a structured payload to *all interested* widgets.

---

### Layer 4: ContextActions (Implemented ✅)

**Mechanism:** Widgets register `ContextAction` structs with `SelectionContext` at startup. Each action has an `is_applicable` predicate that is evaluated dynamically against the current selection state. Any widget can query `applicableActions()` for the current context. Actions appear in right-click context menus.

**Implementation:** `ContextAction` struct in `src/WhiskerToolbox/EditorState/ContextAction.hpp`. `SelectionContext::registerAction()` and `applicableActions()` in `src/WhiskerToolbox/EditorState/SelectionContext.hpp/.cpp`.

**Registered Actions (2):**

| Action ID | Display Name | Registered By | Applicability |
|-----------|-------------|---------------|---------------|
| `scatter_plot.visualize_2d_tensor` | "Visualize in Scatter Plot" | `ScatterPlotWidgetRegistration` | TensorData with ≥2 columns focused |
| `mlcore.cluster_tensor` | "Cluster with ML Workflow" | `MLCoreWidgetRegistration` | Any TensorData focused |

**Architectural Boundary — ContextActions vs Commands:**

| Dimension | Commands (`ICommand`) | Context Actions (`ContextAction`) |
|-----------|----------------------|----------------------------------|
| **Layer** | Data model — mutates `DataManager` | UI orchestration — opens/configures widgets |
| **Applicability** | None — no `poll()` predicate | `is_applicable(SelectionContext const &)` per-action |
| **Serialization** | JSON-serializable via reflect-cpp | **Not serializable** — contains `std::function` captures |
| **Undo** | Opt-in via `isUndoable()` / `undo()` | Not undoable (UI state changes not tracked) |
| **Registration** | `CommandRegistry` explicit dispatcher | `SelectionContext::registerAction()` at widget registration |
| **Examples** | `CopyByTimeRange`, `SaveData` | "Visualize in Scatter Plot", "Cluster with K-Means" |

**Design rule:** If an operation *only* mutates data → Command. If it involves UI orchestration (opening widgets, configuring views) → ContextAction that *may call* Commands internally.

---

## Friction Points

### Friction 1: Column Builder ↔ TransformsV2 Widget (Partially Resolved)

**Resolved:** OperationContext request/response works — ColumnConfigDialog has "Request from Transforms V2" button.

**Remaining friction:**
- **Discoverability:** No visual cue that TransformsV2 can serve as a pipeline source.
- **One-shot:** After delivery, the connection closes. No iteration without re-requesting.
- **Unidirectional:** TransformsV2 sends *to* the column builder. No mechanism to *seed* TransformsV2 with an existing pipeline for editing.

**Resolution:** Proposal 2 (Bidirectional OperationContext) — see below.

### Friction 2: Dimensionality Reduction → Scatter Plot (Resolved ✅)

Resolved by auto-focus via `setDataFocus()` + `scatter_plot.visualize_2d_tensor` ContextAction. One-click from dim reduction output to scatter visualization.

### Friction 3: Scatter Plot Selection → MLCore Clustering (Resolved ✅)

Resolved by `mlcore.cluster_tensor` ContextAction in scatter context menu + selective clustering ("Cluster Selection..." for polygon-selected subsets).

### Friction 4: MLCore Cluster Output → Scatter Recolor (Always Worked ✅)

Works via EntityGroupManager observer → scatter generation counter → scene dirty. No friction — success case for Layer 1/2.

### Friction 5: Multi-Widget Coordinated State (Not Started)

Multi-widget workflows (encoder → PCA → scatter → cluster) are implicit. No save/replay. Deferred to Commands Roadmap Phase 5-6 (RunTransform gateway command + Workflow Pipelines).

---

## Design Inspirations

### Blender: Rich Context, Dumb Editors

Blender's editors are "dumb views" into a rich shared context. Intelligence is in the data model (centralized, dependency-tracked), operators (registered globally, context-aware with `poll()`), and properties (typed, inspectable). Editors render state and invoke operators — they don't coordinate directly.

**Applied to WhiskerToolbox:** The Commands system maps to Blender operators. `SelectionContext` + `DataManager` together serve as context. The gap vs Blender is no dependency DAG — WhiskerToolbox uses fire-and-forget observer notifications rather than smart change propagation.

### VS Code: Commands + When Clauses

VS Code's globally-registered commands with typed arguments and context-key-based visibility ("when clauses") are relevant to discoverability. The Command Palette makes all commands discoverable. WhiskerToolbox's ContextAction system serves the same role — `is_applicable` predicates are the equivalent of VS Code when clauses.

---

## Proposals

### Proposal 1: Context-Aware Actions — IMPLEMENTED ✅

Implemented as described in Layer 4 above. See `ContextAction.hpp`, `SelectionContext::registerAction()`, and the two registered actions (`scatter_plot.visualize_2d_tensor`, `mlcore.cluster_tensor`).

**Future enhancement:** If Commands gain a `canExecute(CommandContext)` predicate, pure-data commands could surface through ContextAction discovery via a thin adapter wrapping Command as ContextAction.

### Proposal 2: Bidirectional OperationContext ("Edit and Return") — Pending

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

**Implementation cost:** Small — need `seed` on `OperationRequestOptions`, `initialSeed()` on `PendingOperation`, producer reads seed, add "Edit in Transforms V2" button alongside existing "Request from Transforms V2."

### Proposal 3: Workflow Pipeline Descriptors — Deferred

**Problem addressed:** Friction 5 — multi-widget workflows are implicit and non-reproducible.

JSON-serializable workflow chaining operations across widgets. Architecturally similar to `CommandSequenceDescriptor` / `TriageSession` pipeline system, but applied to UI workflows. **Deliberately low priority** — Proposals 1 and 2 address the immediate friction. Depends on Commands Phase 5-6 (RunTransform gateway, CLI batch).

---

## Communication Pattern Decision Matrix

| Interaction | Layer | Example |
|-------------|-------|---------|
| "New data exists" | **Layer 1:** DataManager Observer | Transform writes output → scatter refreshes key list |
| "Look at this data" | **Layer 2:** SelectionContext `setDataFocus` | Click key in DataManager_Widget → all widgets update |
| "These entities are selected" | **Layer 2:** SelectionContext `setSelectedEntities` | Select points in scatter → highlight in table |
| "Give me a pipeline config" | **Layer 3:** OperationContext request/response | Column builder ↔ TransformsV2 widget |
| "What can I do with this?" | **Layer 4:** ContextActions | Right-click tensor → "Visualize in Scatter" |
| "Edit this and send back" | **Proposal 2:** Bidirectional OperationContext | Edit existing pipeline JSON in TransformsV2 |
| "Run this whole analysis" | **Proposal 3:** Workflow Pipelines | Encoder → PCA → Scatter → Cluster as one-click |

---

## Ownership & Registration Best Practices

This section answers: as operations grow, **who defines each Command and ContextAction**, and where does the code live?

### Principle: The Provider Defines It

> **The library/widget that implements a capability is the one that defines and registers it.**

This follows from dependency direction: if PCA lives in MLCore, then only MLCore (or code that links MLCore) should define the "Run PCA" operation.

### Command Ownership Tiers

#### Tier 1: Generic CRUD Commands → `src/Commands/`

Data-model operations that work across all data types: `CreateData`, `DeleteData`, `MoveByTimeRange`, `CopyByTimeRange`, `SaveData`/`LoadData`, `ForEachKey`, `SynthesizeData`.

#### Tier 2: Gateway Commands → `src/Commands/`

Thin commands that delegate to an existing execution system. The command provides the serializable/recordable/undoable wrapper; the execution system provides the domain logic:

- `RunTransform` → delegates to TransformsV2 `ElementRegistry` via injected `transform_executor` in `CommandContext`
- `RunPipeline` → delegates to MLCore pipeline execution

**Key insight:** A single `RunTransform` command can surface every TransformsV2 transform without knowing about any of them at compile time (same pattern as `SynthesizeData` → `GeneratorRegistry`).

#### Tier 3: Domain-Specific Commands → Domain Libraries

Complex multi-step operations that don't fit the gateway pattern. Each domain library defines a `register_*_commands()` function, registered via the explicit dispatcher in `register_all_commands.cpp`. See [Commands Roadmap Phase 7](DataManager/Commands/roadmap.qmd#phase-7--command-registration-api-complete).

### ContextAction Ownership: Widgets Define What They Provide

ContextActions belong to the **widget that provides the capability**, registered during widget type registration in the widget's `*Registration.cpp` file. Only the widget knows what data shapes it can consume and how to configure itself.

**Implemented examples:**
- `ScatterPlotWidgetRegistration.cpp` registers `scatter_plot.visualize_2d_tensor`
- `MLCoreWidgetRegistration.cpp` registers `mlcore.cluster_tensor`

### Summary Table

| What | Defined By | Registered Where | Mechanism |
|------|-----------|-----------------|-----------|
| Generic CRUD commands | `src/Commands/` | `CommandRegistry` | Explicit dispatcher (`register_core_commands()`) |
| Gateway commands | `src/Commands/` | `CommandRegistry` | Delegates to domain registry via injected executor |
| Domain commands | Domain library | `CommandRegistry` | Explicit dispatcher (`register_*_commands()`) |
| Transforms | `src/TransformsV2/` | `ElementRegistry` | RAII static `RegisterTransform<>` |
| ContextActions | Provider widget | `SelectionContext` | Widget registration function |

---

## Cross-Roadmap Implementation Sequence

The three roadmaps — [Commands](DataManager/Commands/roadmap.qmd), [Tensor Transforms](tensor_transforms_roadmap.md), and this document — are interdependent.

### Completed Tiers

| Tier | Steps | Status |
|------|-------|--------|
| **Tier 1: Foundation** | Commands Phase 7 (CommandRegistry), Tensor Phase 1 (PCA) | ✅ Complete |
| **Tier 2: Interactive Selection** | Tensor Phase 2 (scatter selection → groups), auto-focus output | ✅ Complete |
| **Tier 3: ContextAction Framework** | `ContextAction` struct, `registerAction()`, `applicableActions()`, two registered actions | ✅ Complete |
| **Tier 4: Integrated UI** | MLCore Dim Reduction tab (4a), clustering ↔ scatter loop (4b) | ✅ Complete (4a, 4b). **4c pending** (Bidirectional OperationContext) |

### Remaining Tiers

#### Tier 4c: Bidirectional OperationContext (Proposal 2)

| Step | Description | Delivers |
|------|-------------|----------|
| Add `seed` + `keep_open_for_iteration` to `OperationRequestOptions` | Extend existing `OperationContext` infrastructure | Friction 1 fully resolved |
| "Edit in Transforms V2" button in ColumnConfigDialog | Seeds TransformsV2_Widget with existing pipeline JSON | Bidirectional pipeline editing |

#### Tier 5: Commands Integration & Reproducibility

| Step | Source Roadmap | Description | Delivers |
|------|---------------|-------------|----------|
| **5a** | Commands (gateway) | `RunTransform` gateway command | Transforms are serializable/replayable |
| **5b** | Commands Phase 3.5 | Wire `RunTransform` into `CommandRecorder` | Transform operations in command log |
| **5c** | Tensor Phase 3 | Tapkee dependency; t-SNE, manifold methods | Non-linear dim reduction |

#### Tier 6: Advanced (Long-Term)

| Step | Source Roadmap | Description | Delivers |
|------|---------------|-------------|----------|
| **6a** | Proposal 3 | Workflow Pipeline Descriptors | One-click replay of multi-widget workflows |
| **6b** | Tensor Phase 7 | Fit/project, scree plot, 3D scatter | Production ML tooling |
| **6c** | Commands Phase 5 | Undo/Redo with `CommandStack` | Undoable transform + grouping operations |

### Friction Resolution Summary

| Friction | Resolution | Status |
|----------|-----------|--------|
| **F1:** Column Builder ↔ TransformsV2 | Tier 4c: Bidirectional OperationContext | ⏳ Pending |
| **F2:** Dim Reduction → Scatter | Auto-focus + `scatter_plot.visualize_2d_tensor` ContextAction | ✅ Resolved |
| **F3:** Scatter → MLCore Clustering | `mlcore.cluster_tensor` ContextAction + selective clustering | ✅ Resolved |
| **F4:** Cluster → Scatter Recolor | Layer 1/2 observers | ✅ Always worked |
| **F5:** Multi-widget workflow replay | Tier 5a-b + Tier 6a | 🔲 Not started |

---

## Relationship to Tensor Refactor Roadmap

The `tensor_roadmap.md` item #1 (interval-row support for non-analog types) has implications for inter-widget communication: once `MaskData`, `LineData`, and `PointData` are supported as interval-row sources for TensorData columns, scatter plot selections on interval-row TensorData will need to map back to the originating intervals. Tracked in `tensor_transforms_roadmap.md` Phase 2, item 3 ("Future extension").

## Implementation Priority Summary

| # | Proposal | Priority | Status |
|---|----------|----------|--------|
| 1 | Context-Aware Actions | **High** | ✅ Implemented |
| 2 | Bidirectional OperationContext | Medium | ⏳ Pending |
| 3 | Workflow Pipeline Descriptors | Low | 🔲 Not started |

## See Also

- [Command Architecture Roadmap](DataManager/Commands/roadmap.qmd) — command foundation, registration, undo/redo
- [Tensor Transforms Roadmap](tensor_transforms_roadmap.md) — PCA, t-SNE, scatter ↔ ML integration
- [Command Architecture Design Document](DataManager/Commands/index.qmd) — full types, patterns, code examples
