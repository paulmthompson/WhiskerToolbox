# Lineage subsystem for DataManager
# Provides container lineage tracking and EntityID propagation
#
# NOTE: LineageTypes.hpp and LineageRegistry.hpp/cpp have been moved to
# src/Entity/Lineage/ as part of the Phase 2 refactoring.
# Only DataManager-specific code remains here:
# - EntityResolver: DataManager-specific entity resolution
# - LineageRecorder: Integration with transform system

# These files are now included directly in DataManager CMakeLists.txt
# No sources defined here
