# GLM-HMM Python Tutorial Roadmap

## Goal

Provide two Quarto how-to guides demonstrating GLM-HMM fitting for whisker contact
detection using the WhiskerToolbox Python bindings. One guide uses the
[ssm](https://github.com/lindermanlab/ssm) library; the other uses
[dynamax](https://github.com/probml/dynamax) (JAX-based).

Both guides follow the same data contract: extract encoder features and labeled
intervals from `DataManager`, fit a GLM-HMM, predict state sequences, and write
results back as `DigitalIntervalSeries`.

## Motivation

See [HMM_WhiskerContact_Analysis.md](HMM_WhiskerContact_Analysis.md) for the
full problem analysis. The standard HMM's fixed transition matrix cannot adapt to
feature-dependent dynamics (e.g., pole position, whisking phase). A GLM-HMM
replaces the static transition matrix with a softmax regression on observed
features, solving this limitation without requiring separate models per condition.

## Data Contract

Both guides assume the following data objects exist in a `DataManager` session:

| Key         | Type                    | Description                                     |
|-------------|-------------------------|-------------------------------------------------|
| `features`  | `TensorData` (2D)      | Shape `[T, D]`, encoder features per frame      |
| `train`     | `DigitalIntervalSeries` | Training block intervals (start/end frames)     |
| `contact`   | `DigitalIntervalSeries` | Contact intervals within training blocks        |

Output: a new `DigitalIntervalSeries` written back to `DataManager` with
predicted contact intervals from the GLM-HMM Viterbi path.

## Python Binding API Summary

```python
import whisker_toolbox as wt

dm = wt.DataManager()

# --- Read features ---
features = dm.getData("features")  # TensorData
X = features.values                # zero-copy numpy (T, D), read-only
X = X.copy()                       # if mutation needed

# --- Read intervals ---
train = dm.getData("train")        # DigitalIntervalSeries
for iv in train.toList():
    print(iv.start, iv.end)        # int64 frame indices

# --- Write results ---
result = wt.DigitalIntervalSeries()
result.addInterval(start, end)
dm.setData("glm_hmm_contact", result, "time_key")
```

## Tasks

### Task 0: Add `TensorData.fromNumpy2D` Python binding

**Status:** ✅ Complete

Add a static factory method to the Python `TensorData` class that accepts a
2D NumPy array and optional column names, returning a new `TensorData`.
This simplifies the numpy → TensorData path for users writing Python scripts.

**Implementation:**
- In `src/python_bindings/bind_tensor.cpp`, add a `def_static("fromNumpy2D", ...)`
  binding that:
  1. Accepts `py::array_t<float, py::array::c_style | py::array::forcecast>`
     and optional `std::vector<std::string>` column names.
  2. Uses `numpy_to_vector<float>()` to copy data into `std::vector<float>`.
  3. Calls `TensorData::createOrdinal2D(data, rows, cols, column_names)`.
  4. Returns `std::shared_ptr<TensorData>`.

**Usage after implementation:**
```python
import numpy as np
import whisker_toolbox as wt

arr = np.random.randn(1000, 192).astype(np.float32)
tensor = wt.TensorData.fromNumpy2D(arr, ["feat_0", "feat_1", ...])
dm.setData("my_features", tensor, "time_key")
```

### Task 1: Write `glm_hmm_ssm.qmd` how-to guide

**Status:** ✅ Complete

Created `docs/learning-resources/how-to/glm_hmm_ssm.qmd` and added it to
`docs/_quarto.yml` under "How-to Guides".

**Contents:**
1. Overview explaining standard HMM limitations and GLM-HMM advantages
2. Data contract (features, train, contact)
3. Pipeline diagram
4. Step-by-step Python code:
   - Extract features and labels from DataManager
   - Build binary label arrays from intervals
   - Create GLM-HMM: `observations="input_driven_obs"`, `C=2`, `transitions="standard"`
   - Fit with EM (including multiple initialisation best-of-N pattern)
   - Viterbi decode and inspect posterior state probabilities
   - Inspect learned transition matrix and per-state GLM weights
   - Forward-filter prediction on unlabeled data (no labels needed)
   - Convert binary predictions to intervals and write back to DataManager
5. Leave-one-block-out cross-validation for model selection (K)
6. Troubleshooting table
7. References (Ashwood et al. 2022, ssm notebook)

### Task 2: Write `glm_hmm_dynamax.qmd` how-to guide

**Status:** ✅ Complete

Created `docs/learning-resources/how-to/glm_hmm_dynamax.qmd` and added it to
`docs/_quarto.yml` under "How-to Guides".

**Contents:**
1. Overview explaining GLM-HMM advantages + why dynamax over ssm (Python 3.12 compat)
2. Data contract (features, train, contact) — same as Task 1
3. Pipeline diagram
4. Step-by-step Python code:
   - Extract features and labels from DataManager
   - Build binary label arrays, standardise features, pad sequences for batched EM
   - Create GLM-HMM: `CategoricalRegressionHMM(num_states, num_classes=2, input_dim=D)`
   - Fit with EM (`model.fit_em(params, props, emissions, inputs)`) including multi-init
   - Alternative SGD fitting with `fit_sgd`
   - Viterbi decode and posterior smoothing
   - Inspect transition matrix and per-state GLM weight contrasts
   - Forward-filter prediction on unlabeled data (no labels needed)
   - Convert binary predictions to intervals and write back to DataManager
5. Leave-one-block-out cross-validation for model selection (K)
6. Troubleshooting table
7. ssm vs dynamax comparison table
8. References (Ashwood et al. 2022, dynamax docs)

**Key dynamax class:** `CategoricalRegressionHMM` — exact equivalent of ssm's
`HMM(..., observations="input_driven_obs", observation_kwargs=dict(C=2))`.
Uses immutable `(params, props)` tuples instead of mutable model objects.

### Task 3: Update `docs/_quarto.yml`

**Status:** ✅ Complete

Both guides added to the "How-to Guides" section in the Learning Resources sidebar:
```yaml
- text: "GLM-HMM Contact Detection (ssm)"
  href: learning-resources/how-to/glm_hmm_ssm.qmd
- text: "GLM-HMM Contact Detection (dynamax)"
  href: learning-resources/how-to/glm_hmm_dynamax.qmd
```

## Dependencies

- **Task 0** is independent; improves ergonomics for Tasks 1–2 but not strictly required.
- **Tasks 1 and 2** are independent of each other.
- **Task 3** depends on Tasks 1 and 2 (files must exist before adding to nav).
- User will handle installing ssm/dynamax Python environments.
