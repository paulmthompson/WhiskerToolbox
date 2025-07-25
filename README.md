![build](https://github.com/paulmthompson/WhiskerToolbox/actions/workflows/linux_cmake.yml/badge.svg)
![build](https://github.com/paulmthompson/WhiskerToolbox/actions/workflows/mac_cmake.yml/badge.svg)
![build](https://github.com/paulmthompson/WhiskerToolbox/actions/workflows/windows_cmake.yml/badge.svg)
[![Coverage Status](https://coveralls.io/repos/github/paulmthompson/WhiskerToolbox/badge.svg?branch=main)](https://coveralls.io/github/paulmthompson/WhiskerToolbox?branch=main)

# WhiskerToolbox

7/25 9:10
Refactor: Improve code readability, safety, and consistency

- Added [[nodiscard]] to hasPreviewMaskData() and getPreviewMaskData() to prevent unintentional result discard.
- Updated _needsTimeFrameConversion() to match header declaration and pass second parameter by const reference.
- Used const-qualified variables (e.g., `const float`, `const QColor`, `const qreal`) where applicable for clarity and immutability.
- Applied std::as_const to loops over internal containers (_line_paths, _masks, _mask_bounding_boxes, _mask_outlines) to prevent accidental mutation.
- Changed loop variable in digital event key search to `bool const in_group` for consistency with const usage.
- Modified UI layout file to:
  - Add missing cursor property (`ArrowCursor`)
  - Add new "Purple" theme option to the appearance selector.
 
7/25 10:25
Refactor: add const qualifiers for improved clarity and const-correctness

- Added const to local variables such as colors, positions, and brushes
- Ensured immutability for values that do not change after initialization
- Improved code clarity and enforced const-correctness

7/25 4:42
Refactor: Apply const correctness and [[nodiscard]] to UI logic and helper methods

- Added `const` qualifiers to local variables where values are not modified (e.g., `QColor`, `QString`, `bool`, `float`, `CanvasCoordinates`, `MediaCoordinates`) across multiple UI components.
- Marked key const-returning methods with `[[nodiscard]]` (e.g., `_generateOutputName`, `getHexColor`) to ensure their return values are not accidentally ignored.
- Improved signal-emitting blocks by marking `CanvasCoordinates` and `MediaCoordinates` as `const` to clarify immutability and intent.
- Updated coordinate handling in mouse event logic for clarity and consistency.
- Enforced `const` on filter helper method signatures and local variables in filter configuration logic.
- Applied `const` to loop-scoped and lambda-captured variables in image/mask processing (e.g., scaling factors, image data).
- Improved maintainability, type safety, and potential for compiler optimization by eliminating unnecessary mutability throughout the codebase.
  

