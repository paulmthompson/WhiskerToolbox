# Lineage subsystem for DataManager
# Provides container lineage tracking and EntityID propagation

set(LINEAGE_SOURCES
    LineageRegistry.cpp
)

set(LINEAGE_HEADERS
    LineageTypes.hpp
    LineageRegistry.hpp
)

# For now, just add to parent directory's sources
# These files will be included in DataManager library build
