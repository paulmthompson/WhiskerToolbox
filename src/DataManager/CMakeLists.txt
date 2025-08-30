

find_package(nlohmann_json CONFIG REQUIRED)

if (ENABLE_OPENCV)
# OPEN CV INTERFERES WITH TENSOR DATA. we need to import it first
    find_package(OpenCV CONFIG REQUIRED) 
endif()

find_package(Armadillo REQUIRED)

find_package(LAPACK REQUIRED)
find_package(OpenBLAS REQUIRED)

add_subdirectory(Observer)

add_subdirectory(TimeFrame)

add_subdirectory(Entity)

add_subdirectory(Lines)

add_subdirectory(IO)

add_subdirectory(Media)

add_subdirectory(Tensors)

set(DATAMANAGER_SOURCES
        DataManager.hpp
        DataManager.cpp
        DataManagerTypes.hpp
        DataManagerTypes.cpp
        ConcreteDataFactory.hpp
        ConcreteDataFactory.cpp

        DigitalTimeSeries/Digital_Interval_Series.hpp
        DigitalTimeSeries/Digital_Interval_Series.cpp
        DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp
        DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.cpp
        DigitalTimeSeries/IO/Binary/Digital_Interval_Series_Binary.hpp
        DigitalTimeSeries/IO/Binary/Digital_Interval_Series_Binary.cpp
        DigitalTimeSeries/IO/JSON/Digital_Interval_Series_JSON.hpp
        DigitalTimeSeries/IO/JSON/Digital_Interval_Series_JSON.cpp
        DigitalTimeSeries/Digital_Event_Series.hpp
        DigitalTimeSeries/Digital_Event_Series.cpp
        DigitalTimeSeries/IO/CSV/Digital_Event_Series_CSV.cpp
        DigitalTimeSeries/IO/CSV/Digital_Event_Series_CSV.hpp
        DigitalTimeSeries/IO/JSON/Digital_Event_Series_JSON.hpp
        DigitalTimeSeries/IO/JSON/Digital_Event_Series_JSON.cpp

        loaders/CSV_Loaders.cpp
        loaders/CSV_Loaders.hpp
        loaders/binary_loaders.hpp
        loaders/loading_utils.hpp
        loaders/loading_utils.cpp

        utils/armadillo_wrap/analog_armadillo.hpp
        utils/armadillo_wrap/analog_armadillo.cpp
        utils/color.hpp
        utils/color.cpp
        utils/json_helpers.hpp
        utils/map_timeseries.hpp
        utils/metaprogramming_utils.hpp
        utils/polynomial/polynomial_fit.hpp
        utils/polynomial/polynomial_fit.cpp
        utils/polynomial/parametric_polynomial_utils.cpp
        utils/polynomial/parametric_polynomial_utils.hpp
        utils/string_manip.hpp
        utils/DataAggregation/DataAggregation.hpp
        utils/DataAggregation/DataAggregation.cpp
        utils/filter/FilterFactory.hpp
        utils/filter/FilterFactory.cpp
        utils/filter/FilterImplementations.hpp
        utils/filter/IFilter.hpp
        utils/filter/ZeroPhaseDecorator.hpp        
        utils/TableView/core/ExecutionPlan.h
        utils/TableView/core/ExecutionPlan.cpp
        utils/TableView/core/RowDescriptor.h
        utils/TableView/core/TableView.h
        utils/TableView/core/TableView.cpp
        utils/TableView/core/TableViewBuilder.h
        utils/TableView/core/TableViewBuilder.cpp
        utils/TableView/core/DataSourceNameInterner.hpp

        utils/TableView/TableInfo.hpp
        utils/TableView/TableEvents.hpp
        utils/TableView/TableRegistry.hpp
        utils/TableView/TableRegistry.cpp
        utils/TableView/columns/IColumn.h
        utils/TableView/columns/Column.h
        utils/TableView/columns/Column.cpp
        utils/TableView/columns/ColumnTypeInfo.hpp

        utils/TableView/pipeline/TablePipeline.hpp
        utils/TableView/pipeline/TablePipeline.cpp

        utils/TableView/interfaces/IAnalogSource.h
        utils/TableView/interfaces/IEventSource.h
        utils/TableView/interfaces/IIntervalSource.h
        utils/TableView/interfaces/ILineSource.h
        utils/TableView/interfaces/IPointSource.h
        utils/TableView/interfaces/IRowSelector.h
        utils/TableView/interfaces/IRowSelector.cpp
        utils/TableView/interfaces/IColumnComputer.h
        utils/TableView/interfaces/IMultiColumnComputer.h
        utils/TableView/interfaces/MultiComputerOutputView.hpp

        utils/TableView/adapters/AnalogDataAdapter.h
        utils/TableView/adapters/AnalogDataAdapter.cpp
        utils/TableView/adapters/DigitalEventDataAdapter.h
        utils/TableView/adapters/DigitalEventDataAdapter.cpp
        utils/TableView/adapters/DigitalIntervalDataAdapter.h
        utils/TableView/adapters/DigitalIntervalDataAdapter.cpp
        utils/TableView/adapters/LineDataAdapter.h
        utils/TableView/adapters/LineDataAdapter.cpp
        utils/TableView/adapters/PointComponentAdapter.h
        utils/TableView/adapters/PointComponentAdapter.cpp
        utils/TableView/adapters/PointDataAdapter.h
        utils/TableView/adapters/PointDataAdapter.cpp
        utils/TableView/adapters/DataManagerExtension.h
        utils/TableView/adapters/DataManagerExtension.cpp

        utils/TableView/computers/IntervalReductionComputer.h
        utils/TableView/computers/IntervalReductionComputer.cpp
        utils/TableView/computers/IntervalOverlapComputer.h
        utils/TableView/computers/IntervalOverlapComputer.cpp
        utils/TableView/computers/IntervalPropertyComputer.h 
        utils/TableView/computers/IntervalPropertyComputer.cpp
        utils/TableView/computers/EventInIntervalComputer.h
        utils/TableView/computers/EventInIntervalComputer.cpp
        utils/TableView/computers/TimestampValueComputer.h
        utils/TableView/computers/TimestampValueComputer.cpp
        utils/TableView/computers/LineSamplingMultiComputer.h
        utils/TableView/computers/AnalogTimestampOffsetsMultiComputer.h
        utils/TableView/computers/AnalogTimestampOffsetsMultiComputer.cpp
        utils/TableView/computers/AnalogSliceGathererComputer.h
        utils/TableView/computers/TimestampInIntervalComputer.h
        utils/TableView/computers/TimestampInIntervalComputer.cpp
        utils/TableView/computers/StandardizeComputer.h

        utils/TableView/ComputerRegistry.cpp 
        utils/TableView/ComputerRegistry.hpp
        utils/TableView/ComputerRegistryTypes.hpp

        utils/TableView/transforms/ITableTransform.hpp
        utils/TableView/transforms/PCATransform.hpp
        utils/TableView/transforms/PCATransform.cpp

        transforms/data_transforms.cpp
        transforms/data_transforms.hpp
        transforms/TransformRegistry.hpp
        transforms/TransformRegistry.cpp
        transforms/TransformPipeline.hpp
        transforms/TransformPipeline.cpp
        transforms/ParameterFactory.hpp
        transforms/ParameterFactory.cpp
        transforms/AnalogTimeSeries/Analog_Event_Threshold/analog_event_threshold.cpp
        transforms/AnalogTimeSeries/Analog_Event_Threshold/analog_event_threshold.hpp
        transforms/AnalogTimeSeries/AnalogFilter/analog_filter.cpp
        transforms/AnalogTimeSeries/AnalogFilter/analog_filter.hpp
        transforms/AnalogTimeSeries/Analog_Interval_Threshold/analog_interval_threshold.cpp
        transforms/AnalogTimeSeries/Analog_Interval_Threshold/analog_interval_threshold.hpp
        transforms/AnalogTimeSeries/AnalogHilbertPhase/analog_hilbert_phase.hpp
        transforms/AnalogTimeSeries/AnalogHilbertPhase/analog_hilbert_phase.cpp
        transforms/AnalogTimeSeries/analog_scaling.cpp
        transforms/AnalogTimeSeries/analog_scaling.hpp
        transforms/DigitalIntervalSeries/Digital_Interval_Group/digital_interval_group.cpp
        transforms/DigitalIntervalSeries/Digital_Interval_Group/digital_interval_group.hpp
        transforms/Lines/Line_Angle/line_angle.hpp
        transforms/Lines/Line_Angle/line_angle.cpp
        transforms/Lines/Line_Min_Point_Dist/line_min_point_dist.hpp
        transforms/Lines/Line_Min_Point_Dist/line_min_point_dist.cpp
        transforms/Lines/Line_Alignment/line_alignment.hpp
        transforms/Lines/Line_Alignment/line_alignment.cpp
        transforms/Lines/Line_Resample/line_resample.cpp
        transforms/Lines/Line_Resample/line_resample.hpp
        transforms/Lines/line_curvature.cpp
        transforms/Lines/line_curvature.hpp
        transforms/Lines/line_subsegment.cpp
        transforms/Lines/line_subsegment.hpp
        transforms/Lines/line_point_extraction.cpp
        transforms/Lines/line_point_extraction.hpp
        transforms/Lines/line_clip.cpp
        transforms/Lines/line_clip.hpp
        transforms/Masks/Mask_Area/mask_area.cpp
        transforms/Masks/Mask_Area/mask_area.hpp
        transforms/Masks/Mask_Centroid/mask_centroid.cpp
        transforms/Masks/Mask_Centroid/mask_centroid.hpp
        transforms/Masks/Mask_Connected_Component/mask_connected_component.cpp
        transforms/Masks/Mask_Connected_Component/mask_connected_component.hpp
        transforms/Masks/Mask_Hole_Filling/mask_hole_filling.cpp
        transforms/Masks/Mask_Hole_Filling/mask_hole_filling.hpp
        transforms/Masks/Mask_Median_Filter/mask_median_filter.cpp
        transforms/Masks/Mask_Median_Filter/mask_median_filter.hpp
        transforms/Masks/Mask_Principal_Axis/mask_principal_axis.cpp
        transforms/Masks/Mask_Principal_Axis/mask_principal_axis.hpp
        transforms/Masks/mask_to_line.cpp
        transforms/Masks/mask_to_line.hpp
        transforms/Masks/mask_skeletonize.cpp
        transforms/Masks/mask_skeletonize.hpp
        transforms/Media/whisker_tracing.cpp
        transforms/Media/whisker_tracing.hpp
)


set_property(GLOBAL PROPERTY DATAMANAGER_TEST_SOURCES "")

add_subdirectory(AnalogTimeSeries)
add_subdirectory(Masks)
add_subdirectory(Points)

add_library(DataManager SHARED ${DATAMANAGER_SOURCES}
    )


target_include_directories(DataManager PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
        "${iir_SOURCE_DIR}"   
    )


target_link_libraries(DataManager PUBLIC nlohmann_json::nlohmann_json)

target_link_libraries(DataManager PRIVATE Whisker-Analysis)

target_link_libraries(DataManager PUBLIC iir::iir)

target_link_libraries(DataManager PUBLIC WhiskerToolbox::TensorData)


target_link_libraries(DataManager PUBLIC WhiskerToolbox::ObserverData)
target_link_libraries(DataManager PUBLIC WhiskerToolbox::TimeFrame)
target_link_libraries(DataManager PUBLIC WhiskerToolbox::Entity)
target_link_libraries(DataManager PUBLIC WhiskerToolbox::LineData)
target_link_libraries(DataManager PUBLIC WhiskerToolbox::MaskData)
target_link_libraries(DataManager PUBLIC WhiskerToolbox::MediaData)
target_link_libraries(DataManager PUBLIC NEURALYZER_GEOMETRY)
if(ENABLE_OPENCV)
    target_link_libraries(DataManager PUBLIC ImageProcessing)
endif()

target_link_libraries(DataManager PRIVATE DataManagerIO)


if(ENABLE_HDF5)
    target_link_libraries(DataManager PRIVATE DataManagerHDF5)
    target_compile_definitions(DataManager PRIVATE ENABLE_HDF5)
endif()

if(ENABLE_CAPNPROTO)
    target_link_libraries(DataManager PRIVATE DataManagerIO_CapnProto)
    target_compile_definitions(DataManager PRIVATE ENABLE_CAPNPROTO)
endif()

if(ENABLE_OPENCV)
    target_link_libraries(DataManager PRIVATE DataManagerOpenCV)
    target_compile_definitions(DataManager PRIVATE ENABLE_OPENCV)
endif()

#It is important to put these last to avoid linker errors with no OpenCV
#I don't know exactly why
target_link_libraries(DataManager PRIVATE armadillo)
target_link_libraries(DataManager PRIVATE LAPACK::LAPACK)
target_link_libraries(DataManager PRIVATE OpenBLAS::OpenBLAS)


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataManager PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataManager PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DataManager PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(DataManager PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
