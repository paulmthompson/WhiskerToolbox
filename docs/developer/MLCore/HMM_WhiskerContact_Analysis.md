# HMM Performance Analysis: Whisker Contact Detection at High Sampling Rates

## Problem Statement

Encoder features (192-dim via ConvNeXt + GlobalAvgPool) are extracted per-frame from video, then an HMM classifies contact intervals. Two experimental configurations show dramatically different performance:

| Configuration | Video resolution | Encoder input | Sampling rate | Performance |
|---|---|---|---|---|
| **A** (640×480) | 640×480 | 256×256 (resized) | ~500 Hz | Good |
| **B** (256×256) | 256×256 | 256×256 (native) | ~2000 Hz | Poor |

Features look similar by visual inspection. No bug was found in the image encoding or resize path.

## Root Causes

### 1. Transition matrix becomes too sticky at high sampling rates

Supervised HMM training counts state transitions directly from labeled sequences. With the same physical interval duration:

- **500 Hz**: 50-frame block → self-transition $A_{ii} \approx 49/50 = 0.98$, switch penalty $\log(0.02) \approx -3.9$
- **2000 Hz**: 200-frame block → self-transition $A_{ii} \approx 199/200 = 0.995$, switch penalty $\log(0.005) \approx -5.3$

The model becomes far more reluctant to transition. Short intervals get swallowed, and transition boundaries shift.

### 2. Full covariance is underdetermined at 192 dimensions

With $D = 192$ features and $K$ states:

| Covariance mode | Params per state | Total for K=2 |
|---|---|---|
| **Full** | $192 + 192 \times 193 / 2 = 18{,}720$ | **37,440** |
| **Diagonal** | $192 + 192 = 384$ | **768** |

At 2000 Hz with ~12,000 samples and 2 states, that's ~6,000 samples per state—but consecutive frames are nearly identical due to temporal autocorrelation, so the effective independent sample count is much lower. Full covariance estimation at 192 dims requires ~18,528 independent samples per state. This is massively underdetermined.

**Fix:** Use `use_diagonal_covariance = true` in `HMMParameters`, and/or apply PCA to reduce from 192 to 20–30 dimensions before HMM fitting.

### 3. The 2-state model is misspecified for whisker dynamics

Whisker contact involves three behavioral regimes, not two:

| Regime | Contact? | Duration at 2000 Hz | Transition behavior |
|---|---|---|---|
| **Quiescent** | No | Thousands of frames | Very sticky |
| **Whisking, retracted** | No | 50–100 frames (half-cycle at 10–20 Hz) | Fast oscillation with contact |
| **Whisking, in contact** | Yes | 50–100 frames | Fast oscillation with retracted |

A 2-state HMM forces one transition matrix to explain both the very slow quiescent↔whisking transitions AND the fast 10–20 Hz contact oscillations. It compromises on an intermediate transition probability that fits neither regime.

**Fix:** Use 3 states with combined interval labels (see below).

### 4. Pole position modulates transition dynamics

Different pole positions produce qualitatively different contact patterns:

- **Anterior pole**: Brief contacts at whisk peaks, fast transitions
- **Posterior pole**: Sustained contacts spanning multiple whisk cycles, slow transitions

A single HMM averages these into one compromise transition matrix.

**Fix:** Train separate HMMs per pole condition (simplest), or use a model with covariate-dependent transitions (see Advanced Models).

## Recommended Configuration

1. **Diagonal covariance** (`use_diagonal_covariance = true`)
2. **3-state combined labels** from two interval series:
   - Whisking intervals + Contact intervals → Combined state
   - `(whisking=0, contact=0) → state 0` (quiescent)
   - `(whisking=1, contact=0) → state 1` (whisking, no contact)
   - `(whisking=1, contact=1) → state 2` (whisking, contact)
   - After prediction, decompose: `state 2 → contact`, `states 0,1 → no contact`
3. **Separate HMMs per pole position** — each gets its own transition matrix reflecting its actual dynamics
4. **Optional PCA** from 192 → 20–30 dimensions before HMM if diagonal covariance alone isn't sufficient

## Combined Interval Composition (General Pattern)

Given $N$ binary interval series, produce $2^N$ combined HMM states. This is a general abstraction:

```
combined_state = Σ(interval_i × 2^i)  for i in 0..N-1
```

After HMM prediction, decompose back via the same mapping. This allows encoding complex multi-interval behavioral structure into the HMM state space while maintaining a clean label interface.

## Advanced State-Space Models

For cases where transition dynamics vary by a known continuous covariate (e.g., pole position), several models go beyond standard HMMs:

### Input-Output HMM (IOHMM)

The transition matrix is a function of observed inputs:

$$P(z_t = j \mid z_{t-1} = i, \mathbf{u}_t) = \frac{\exp(\mathbf{w}_{ij}^\top \mathbf{u}_t)}{\sum_k \exp(\mathbf{w}_{ik}^\top \mathbf{u}_t)}$$

Each row of the transition matrix is a softmax regression on the covariate. This is the most direct solution to "transition dynamics vary smoothly with pole position."

**Available in:** `ssm` (Python, Linderman lab), `dynamax` (JAX)

### Hidden Semi-Markov Model (HSMM)

Replaces the implicit geometric dwell-time distribution with explicit duration distributions per state. Can specify that anterior-contact lasts 25–50 ms while posterior-contact lasts 100+ ms.

**Available in:** `pyhsmm` (Python), `hsmm` (R)

### GLM-HMM

Both emissions and transitions are generalized linear models on covariates. Used extensively in systems neuroscience (International Brain Lab, Churchland lab).

**Available in:** `ssm` (Python), `glm-hmm` (Python, Churchland lab)

### Switching Linear Dynamical System (SLDS)

Combines discrete HMM states with per-state linear dynamics on continuous observations. Naturally handles temporal autocorrelation. Overkill for contact detection, appropriate if modeling whisker kinematics directly.

**Available in:** `ssm` (Python), `dynamax` (JAX)

### Sticky HDP-HMM

Nonparametric—automatically infers the number of states. The "sticky" variant adds learned self-transition bias per state, so quiescent states can be sticky while whisking states oscillate.

**Available in:** `pyhsmm` (Python), `ssm` (Python)

### Summary Table

| Model | Covariate-dependent transitions | Explicit durations | Auto state count | Available in mlpack |
|---|---|---|---|---|
| Standard HMM | No | No | No | **Yes** |
| IOHMM | **Yes** | No | No | No |
| HSMM | No | **Yes** | No | No |
| GLM-HMM | **Yes** | No | No | No |
| SLDS | No | No | No | No |
| HDP-HMM | No | No | **Yes** | No |

The `PythonBridge` module in WhiskerToolbox could be used to call Python implementations of these models if needed.

## Why Diverse Negatives Hurt HMMs (But Help Classifiers)

### Discriminative classifiers benefit from heterogeneous negatives

A frame-level classifier (SVM, random forest, logistic regression) learns a **decision boundary** in feature space. Adding "no pole present" frames to the "no contact" class is purely beneficial — it expands the classifier's understanding of what non-contact looks like. The classifier doesn't care *why* there's no contact. More negative variety = better boundary = fewer false positives.

### HMMs are harmed by heterogeneous negatives

The HMM jointly estimates three things per state:

1. **Emission distribution**: what features look like in this state
2. **Transition probabilities**: how long this state lasts and what follows it
3. **Initial distribution**: which state the sequence starts in

If "no pole present" frames are added to the "no contact" state:

- **Emissions get corrupted**: The "no contact" distribution becomes a mixture of (a) quiescent with no pole, (b) whisking past where the pole would be, and (c) whisking near the pole but not touching. These have different feature profiles. The emission Gaussian becomes broad and uninformative — it overlaps with genuine contact features.

- **Transitions get corrupted**: "No pole" periods are 100% self-transition for no-contact. Mixing those with whisking periods (which have rapid contact↔no-contact oscillations) averages the transition rates toward something that describes neither regime.

- **Net effect**: The HMM becomes *less* confident about everything. The emission distributions are blurry (high variance from mixing conditions), and the transition matrix is a meaningless average of two very different dynamics.

**Takeaway:** HMM training data should be **condition-matched** — only include frames from the same pole condition (or dynamically similar periods). Adding "easy negatives" from no-pole trials dilutes the emission estimates rather than improving them.

## Two Separate Limitations of Standard Gaussian HMMs

### Limitation 1: Fixed transition matrix (the bigger issue)

In a standard HMM, $P(z_t \mid z_{t-1})$ is a **constant matrix** — it doesn't see the features at all. The model has no mechanism to say "given these features, I should transition." It can only say "on average across all training data, this is how often state A follows state A."

This is why diverse negatives hurt: the transition row for "no contact" averages quiescent dynamics (almost never transition) with whisking dynamics (transition every 50 frames). The model can't distinguish these at transition time because **it doesn't look at the features when deciding to transition.**

The IOHMM / GLM-HMM models fix exactly this by replacing:

$$P(z_t = j \mid z_{t-1} = i) = A_{ij} \quad \text{(constant)}$$

with:

$$P(z_t = j \mid z_{t-1} = i, \mathbf{y}_t) = f(A_{ij}, \mathbf{y}_t) \quad \text{(feature-dependent)}$$

Now the model **can** learn: "when features look like quiescent, self-transition probability is 0.9999" and "when features look like active whisking, self-transition is 0.95." A single "no contact" state could accommodate both sub-regimes because the transition function adapts to the current context.

### Limitation 2: Gaussian emissions (compounding issue)

A single Gaussian per state is unimodal — one mean, one covariance. If "no contact" includes both quiescent frames (low-energy features) and active-whisking-no-contact frames (high-energy features), the Gaussian places its mean somewhere in between and inflates its variance to cover both. This produces:

- Poor likelihood scores for both sub-populations (both are in the tails)
- Overlap with the "contact" emission distribution (the inflated variance bleeds into contact feature space)

Non-parametric or richer emission models help:

| Emission model | Multi-modal? | Captures "no contact" diversity? |
|---|---|---|
| Single Gaussian | No | No — one mean, one variance |
| **Gaussian Mixture (GMM-HMM)** | Yes | Yes — learns sub-clusters automatically |
| Kernel density | Yes | Yes — but scales poorly to 192 dims |
| Neural network (deep HMM) | Yes | Yes — most flexible |

A **GMM-HMM** (each state's emission is a mixture of Gaussians) would let the "no contact" state have one mixture component for quiescent features and another for active-whisking features. mlpack supports `mlpack::HMM<mlpack::GMM>`, though it is not currently exposed in `HiddenMarkovModelOperation`.

### Emissions alone don't solve it

Even with perfect multimodal emissions, the fixed transition matrix remains the bottleneck. A GMM-HMM can correctly identify "these features are quiescent-no-contact" vs. "these features are whisking-no-contact" and assign high emission probability to both. But when deciding $P(\text{transition to contact} \mid \text{currently no contact})$, it uses **the same transition probability** regardless of which mixture component generated the current observation. It still can't capture "quiescent → never transitions" vs. "whisking → transitions at 10–20 Hz."

The full solution requires **both** feature-dependent transitions AND rich emissions — which is what the GLM-HMM provides.

## Discriminative-Generative Cascade (Practical Hybrid)

A practical approach that works with the current codebase:

1. **Train a frame-level discriminative classifier** on all available data (diverse negatives, all pole positions — heterogeneity helps here)
2. **Produce per-frame contact probabilities** (the classifier's output)
3. **Feed the 1D probability signal into a simple 2-state HMM** for temporal smoothing

The classifier handles the emission complexity (multimodal negatives, pole-dependent features). The HMM only needs to learn the temporal dynamics of a 1-dimensional signal with well-calibrated likelihoods. This avoids the Gaussian emission problem entirely and reduces the transition matrix to a simple smoothing role.

## Implementability Summary

| Approach | Solves transitions? | Solves emissions? | Available now? |
|---|---|---|---|
| Separate HMMs per condition | Yes (by partitioning) | Partially | **Yes** |
| 3-state HMM + diagonal cov | Partially | No | **Yes** |
| GMM-HMM (mlpack `HMM<GMM>`) | No | **Yes** | Could expose with modest code |
| GLM-HMM via PythonBridge | **Yes** | **Yes** | Via `ssm` Python library |
| Discriminative-generative cascade | Bypasses both | N/A | **Yes** (train classifier → feed probs to HMM) |
