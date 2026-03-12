if (APPLE)
    message(STATUS "Testing Currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currently not supported on Windows")
    return()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

add_executable(test_commands

        ${CMAKE_SOURCE_DIR}/src/Commands/Core/CommandFactory.test.cpp
        ${CMAKE_SOURCE_DIR}/src/Commands/Core/CommandInfo.test.cpp
        ${CMAKE_SOURCE_DIR}/src/Commands/Core/VariableSubstitution.test.cpp
        ${CMAKE_SOURCE_DIR}/src/Commands/GetEntityIdsInRange.test.cpp
        ${CMAKE_SOURCE_DIR}/src/Commands/Commands.test.cpp
        ${CMAKE_SOURCE_DIR}/src/Commands/IO/SaveData.test.cpp
        ${CMAKE_SOURCE_DIR}/src/Commands/IO/SaveDataDigitalEvent.test.cpp
        ${CMAKE_SOURCE_DIR}/src/Commands/IO/SaveDataDigitalInterval.test.cpp
        ${CMAKE_SOURCE_DIR}/src/Commands/IO/SaveDataAnalog.test.cpp
        ${CMAKE_SOURCE_DIR}/src/Commands/IO/LoadData.test.cpp
        ${CMAKE_SOURCE_DIR}/src/Commands/Core/Validation.test.cpp
        ${CMAKE_SOURCE_DIR}/src/Commands/Core/CommandRecorder.test.cpp
)

target_link_libraries(test_commands PRIVATE Catch2::Catch2WithMain DataManager Commands)

target_include_directories(test_commands PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Commands>"
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/..>"
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/tests/Commands>"
)

if (ENABLE_DETAILED_TEST_DISCOVERY)
    if (WIN32)
        catch_discover_tests(test_commands
                DL_PATHS
                "$ENV{PATH}"
                "${CMAKE_BINARY_DIR}"
        )
    else()
        catch_discover_tests(test_commands)
    endif()
else()
    add_test(NAME test_commands_all
        COMMAND test_commands
        )
endif()