# Third-Party Notices

WhiskerToolbox (Neuralyzer) is licensed under **GPL-2.0-or-later** (see [`LICENSE`](LICENSE)).
This file lists **included third-party source** that is not necessarily under the same
license as the project as a whole.

## Combined work and distribution variants

| Build | GPLv3 components included | Effective license for that binary |
|---|---|---|
| **Full build** (default) | Yes (`rosl.hpp` / Robust PCA) | **GPL-3.0** (combined program) |
| **Build without Robust PCA** | No | **GPL-2.0** compatible (when GPLv3 code is excluded at link time) |

Your own source files remain under **GPL-2.0-or-later**. Third-party files keep the
licenses shown in their headers. Do not replace upstream license headers with GPL-2.

---

## LGPL-3.0-or-later

### Elypson / qt-collapsible-section (Collapsible_Widget)

| | |
|---|---|
| **Upstream** | [MichaelVoelkel/qt-collapsible-section](https://github.com/MichaelVoelkel/qt-collapsible-section) |
| **Copyright** | Michael A. Voelkel (2016) |
| **License** | LGPL-3.0-or-later |
| **Files** | `src/WhiskerToolbox/Collapsible_Widget/Section.cpp`, `Section.hpp` |
| **Integration** | Static library (`Collapsible_Widget`); linked into WhiskerToolbox and several widgets |
| **License text** | <https://www.gnu.org/licenses/lgpl-3.0.html> |

Original LGPL copyright and license blocks in the source files must be preserved.

---

## GPL-3.0-or-later

### robustpca / ROSL (Robust PCA)

| | |
|---|---|
| **Upstream** | Tom Furnival, robustpca (ROSL header-only implementation) |
| **Copyright** | Tom Furnival (2015–2020) |
| **License** | GPL-3.0-or-later |
| **Files** | `src/MLCore/models/unsupervised/rosl.hpp` |
| **Used by** | `RobustPCAOperation.cpp`, `SupervisedRobustPCAOperation.cpp`, related MLCore and TransformsV2 paths |
| **License text** | <https://www.gnu.org/licenses/gpl-3.0.html> |

Distributions that compile and link this code must comply with **GPLv3** for the
combined program. To ship a **GPLv2-compatible** binary, exclude Robust PCA / ROSL
from the build (no compilation or linking of the above paths).

---

## BSD-3-Clause

### scikit-image skeletonize (Zhang–Suen thinning loop)

| | |
|---|---|
| **Upstream** | scikit-image `morphology/_skeletonize_various_cy.pyx` (`_skeletonize_loop` / `_fast_skeletonize`) |
| **Copyright** | Massachusetts Institute of Technology, Broad Institute, Lee Kamentsky (2003–2011) |
| **License** | BSD-3-Clause |
| **Files** | `src/DataObjects/Masks/utils/skeletonize.cpp`, `skeletonize.hpp` |
| **Notes** | Attribution and license notice retained in `skeletonize.cpp` |

### scikit-image medial_axis (single-pass connectivity thinning)

| | |
|---|---|
| **Upstream** | scikit-image `morphology/_skeletonize.py` (`medial_axis`), `_skeletonize_various_cy.pyx` (`_skeletonize_loop`) |
| **Copyright** | Massachusetts Institute of Technology, Broad Institute, scikit-image contributors |
| **License** | BSD-3-Clause |
| **Files** | `src/DataObjects/Masks/utils/medial_axis_skeletonize.cpp`, `medial_axis_skeletonize.hpp` |
| **Notes** | Uses Felzenszwalb distance transform (below) for EDT; attribution in source file |

### Felzenszwalb distance transform

| | |
|---|---|
| **Upstream** | [Distance Transforms of Sampled Functions](https://cs.brown.edu/~pff/papers/dt-final.pdf) reference implementation |
| **Copyright** | Pedro Felzenszwalb (2006) |
| **License** | GPL-2.0-or-later |
| **Files** | `src/DataObjects/Masks/utils/distance_transform.cpp`, `distance_transform.hpp` |
| **Notes** | Adapted from user-provided reference code in `~/Downloads/dt`; original license header retained |

### Kd-Tree (SpatialIndex)

| | |
|---|---|
| **Upstream** | Christoph Dalitz, Jens Wilberg |
| **Copyright** | Christoph Dalitz (2018–2023), Jens Wilberg (2018) |
| **License** | BSD-style (see file headers) |
| **Files** | `src/SpatialIndex/KdTree.hpp`, `KdTree.cpp` |
| **Notes** | Attribution retained in source headers |

---

## MIT License

### Kalman filter (StateEstimation)

| | |
|---|---|
| **Copyright** | Hayk Martirosyan (2014) |
| **License** | MIT |
| **Files** | `src/StateEstimation/Filter/Kalman/kalman.hpp`, `kalman.cpp` |

### ezEngine color scheme

| | |
|---|---|
| **Upstream** | [ezEngine](https://github.com/ezEngine/ezEngine) |
| **Copyright** | ezEngine Project (2024) |
| **License** | MIT |
| **Files** | `src/WhiskerToolbox/color_scheme.hpp` |
| **Notes** | MIT license block retained in source file |

---

## Runtime dependencies (not vendored)

The following are **linked at build time** and are **not** copied into this repository.
Their licenses apply to those separate packages when you distribute binaries:

- **Qt 6** — LGPL-3.0 / commercial (see The Qt Company)
- **OpenCV** (optional) — Apache-2.0
- **Armadillo**, **MLPack**, **libtorch**, and other dependencies pulled via CMake/vcpkg — see each package’s license

Consult your build configuration and vcpkg/conan manifests for the exact dependency set.

---

## Adding new third-party code

When vendoring external source:

1. Keep the original copyright and license header in the file.
2. Add an entry to this document.
3. Confirm compatibility with **GPL-2.0-or-later** (or document if the combined build
   requires **GPLv3** or another notice).
