find_package(reflectcpp CONFIG REQUIRED)

set(COMMAND_SOURCES
    CommandResult.hpp
        CommandContext.hpp
        CommandDescriptor.hpp
        CommandInfo.hpp
        CommandUIHints.hpp
        ICommand.hpp
        CommandFactory.hpp
        CommandFactory.cpp
        VariableSubstitution.hpp
        VariableSubstitution.cpp
        SequenceExecution.hpp
        SequenceExecution.cpp
        GetEntityIdsInRange.hpp
        GetEntityIdsInRange.cpp
        MoveByTimeRange.hpp
        MoveByTimeRange.cpp
        CopyByTimeRange.hpp
        CopyByTimeRange.cpp
        AddInterval.hpp
        AddInterval.cpp
        ForEachKey.hpp
        ForEachKey.cpp
        SaveData.hpp
        SaveData.cpp
        LoadData.hpp
        LoadData.cpp
        Validation.hpp
        Validation.cpp
        CommandRecorder.hpp
        CommandRecorder.cpp
)

add_library(Commands STATIC ${COMMAND_SOURCES})

target_include_directories(Commands PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>" 
    "$<INSTALL_INTERFACE:include>"
)

target_link_libraries(Commands PRIVATE DataManager)
target_link_libraries(Commands PUBLIC reflectcpp::reflectcpp)

set_target_compiler_warnings(Commands)

set_target_properties(Commands PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
enable_whiskertoolbox_pch(Commands)