# FileExplorer_Widgets
# Parent directory for file format explorer widgets.
#
# These widgets allow users to browse and preview data files before importing.
# Each explorer handles a specific file format and registers with
# DataImportTypeRegistry so it appears in the Data Import widget.
#
# Current explorers:
# - NpyExplorer_Widget: Browse NumPy .npy files
# - BinaryExplorer_Widget: Browse raw binary data files
# - CsvExplorer_Widget: Browse CSV/delimited text files

add_subdirectory(NpyExplorer_Widget)
add_subdirectory(BinaryExplorer_Widget)
add_subdirectory(CsvExplorer_Widget)
