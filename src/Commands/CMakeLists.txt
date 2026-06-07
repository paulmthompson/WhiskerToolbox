find_package(reflectcpp CONFIG REQUIRED)

set(COMMAND_SOURCES

    register_core_commands.cpp
    register_core_commands.hpp
    CommandUIHints.hpp

    Core/CommandResult.hpp
    Core/CommandContext.hpp
    Core/CommandDescriptor.hpp
    Core/CommandInfo.hpp
    Core/ICommand.hpp
    Core/CommandFactory.hpp
    Core/CommandFactory.cpp
    Core/CommandRegistry.hpp
    Core/CommandRegistry.cpp
    Core/VariableSubstitution.hpp #Detail?
    Core/VariableSubstitution.cpp #Detail?
    Core/SequenceExecution.hpp
    Core/SequenceExecution.cpp
    Core/Validation.hpp
    Core/Validation.cpp
    Core/CommandRecorder.hpp
    Core/CommandRecorder.cpp

    DataObjects/DigitalTimeSeries/AddInterval.hpp
    DataObjects/DigitalTimeSeries/AddInterval.cpp
    DataObjects/DigitalTimeSeries/FlipEventAtTime.hpp
    DataObjects/DigitalTimeSeries/FlipEventAtTime.cpp
    DataObjects/DigitalTimeSeries/SetEventAtTime.hpp
    DataObjects/DigitalTimeSeries/SetEventAtTime.cpp

    DataObjects/Lines/ClearLineDataAtTime.hpp
    DataObjects/Lines/ClearLineDataAtTime.cpp

        GetEntityIdsInRange.hpp
        GetEntityIdsInRange.cpp
        MoveByTimeRange.hpp
        MoveByTimeRange.cpp
        CopyByTimeRange.hpp
        CopyByTimeRange.cpp
        AdvanceFrame.hpp
        AdvanceFrame.cpp
        ForEachKey.hpp
        ForEachKey.cpp

        IO/SaveData.hpp
        IO/SaveData.cpp
        IO/SavePathResolver.hpp
        IO/SavePathResolver.cpp
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
target_link_libraries(Commands PRIVATE WhiskerToolbox::TimeController)
target_link_libraries(Commands PRIVATE DataManagerIO)
target_link_libraries(Commands PUBLIC reflectcpp::reflectcpp)

target_link_libraries(Commands PRIVATE WhiskerToolbox::AnalogTimeSeries)
target_link_libraries(Commands PRIVATE WhiskerToolbox::DigitalTimeSeries)
target_link_libraries(Commands PRIVATE WhiskerToolbox::PointData)
target_link_libraries(Commands PRIVATE WhiskerToolbox::LineData)
target_link_libraries(Commands PRIVATE WhiskerToolbox::MaskData)
target_link_libraries(Commands PRIVATE WhiskerToolbox::MediaData)
target_link_libraries(Commands PRIVATE WhiskerToolbox::TensorData)

set_target_compiler_warnings(Commands)

set_target_properties(Commands PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
enable_whiskertoolbox_pch(Commands)