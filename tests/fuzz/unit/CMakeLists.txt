# Unit fuzz tests - mirror the src directory structure

# Digital Event Series JSON fuzz test
add_subdirectory(DataManager)

add_subdirectory(TransformsV2)

# EditorState data struct round-trip and crash-resistance fuzz tests
add_subdirectory(EditorState)
