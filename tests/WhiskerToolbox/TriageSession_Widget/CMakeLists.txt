if (APPLE)
    message(STATUS "Testing currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing currently not supported on Windows")
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_executable(TriageSessionState_test
    TriageSessionState.test.cpp
)

set_target_properties(TriageSessionState_test PROPERTIES
    AUTOMOC ON
)

target_link_libraries(TriageSessionState_test PRIVATE
    Catch2::Catch2WithMain
    Qt6::Core
    Qt6::Widgets
    TriageSession_Widget
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(TriageSessionState_test)
else()
    add_test(NAME TriageSessionState_test_all
        COMMAND TriageSessionState_test
    )
endif()

enable_whiskertoolbox_shared_test_pch(TriageSessionState_test)
