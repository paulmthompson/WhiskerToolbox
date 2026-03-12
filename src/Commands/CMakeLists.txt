find_package(reflectcpp CONFIG REQUIRED)

set(COMMAND_SOURCES
    Core/CommandResult.hpp
    Core/CommandContext.hpp
    Core/CommandDescriptor.hpp
    Core/CommandInfo.hpp
        CommandUIHints.hpp
    Core/ICommand.hpp
    Core/CommandFactory.hpp
    Core/CommandFactory.cpp
    Core/VariableSubstitution.hpp #Detail?
    Core/VariableSubstitution.cpp #Detail?
    Core/SequenceExecution.hpp
    Core/SequenceExecution.cpp
    Core/Validation.hpp
    Core/Validation.cpp
    Core/CommandRecorder.hpp
    Core/CommandRecorder.cpp

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

        IO/SaveData.hpp
        IO/SaveData.cpp
        IO/LoadData.hpp
        IO/LoadData.cpp

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