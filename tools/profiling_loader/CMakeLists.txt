cmake_minimum_required(VERSION 3.20)

# Simple executable to profile the load_data_from_json_config function
add_executable(profile_loader profile_loader.cpp)

target_link_libraries(profile_loader PRIVATE 
    DataManager
)

# Copy to build directory for easy access
set_target_properties(profile_loader PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install the executable
install(TARGETS profile_loader
    RUNTIME DESTINATION bin
)
