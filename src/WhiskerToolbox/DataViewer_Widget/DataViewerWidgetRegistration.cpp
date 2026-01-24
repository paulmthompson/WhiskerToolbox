#include "DataViewerWidgetRegistration.hpp"

#include "DataViewer_Widget.hpp"
#include "DataViewerState.hpp"
#include "DataViewerPropertiesWidget.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "DataManager/DataManager.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include <iostream>

namespace DataViewerWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager,
                   TimeScrollBar * time_scrollbar) {
    
    if (!registry) {
        std::cerr << "DataViewerWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;
    auto ts = time_scrollbar;

    registry->registerType({
        .type_id = QStringLiteral("DataViewerWidget"),
        .display_name = QStringLiteral("Data Viewer"),
        .icon_path = QStringLiteral(":/icons/dataviewer.png"),
        .menu_path = QStringLiteral("View/Visualization"),
        .preferred_zone = Zone::Center,
        .properties_zone = Zone::Right,
        .prefers_split = false,
        .properties_as_tab = true,
        .auto_raise_properties = false,
        .allow_multiple = true,

        // State factory - creates the shared state object
        .create_state = []() {
            return std::make_shared<DataViewerState>();
        },

        // View factory - creates DataViewer_Widget (the view component)
        .create_view = [dm, ts](std::shared_ptr<EditorState> state) -> QWidget * {
            auto viewer_state = std::dynamic_pointer_cast<DataViewerState>(state);
            if (!viewer_state) {
                std::cerr << "DataViewerWidgetModule: Failed to cast state to DataViewerState" << std::endl;
                return nullptr;
            }

            auto * widget = new DataViewer_Widget(dm, ts);
            widget->setState(viewer_state);
            
            // Trigger initial setup that was previously done in openWidget()
            widget->openWidget();

            return widget;
        },

        // Properties factory - creates DataViewerPropertiesWidget
        .create_properties = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
            auto viewer_state = std::dynamic_pointer_cast<DataViewerState>(state);
            if (!viewer_state) {
                std::cerr << "DataViewerWidgetModule: Failed to cast state to DataViewerState for properties" << std::endl;
                return nullptr;
            }

            // Create properties widget with shared state
            auto * props = new DataViewerPropertiesWidget(viewer_state, dm);
            return props;
        },

        // Custom editor creation for potential future view/properties coupling
        .create_editor_custom = [dm, ts](EditorRegistry * reg) 
            -> EditorRegistry::EditorInstance 
        {
            // Create the shared state
            auto state = std::make_shared<DataViewerState>();

            // Create the view widget
            auto * view = new DataViewer_Widget(dm, ts);
            view->setState(state);
            
            // Trigger initial setup
            view->openWidget();

            // Create the properties widget with the shared state and OpenGLWidget
            auto * opengl_widget = view->getOpenGLWidget();
            auto * props = new DataViewerPropertiesWidget(state, dm, opengl_widget);
            
            // Connect properties auto-arrange signal to view
            QObject::connect(props, &DataViewerPropertiesWidget::autoArrangeRequested,
                             view, &DataViewer_Widget::autoArrangeVerticalSpacing);
            
            // Connect properties export SVG signal to view
            QObject::connect(props, &DataViewerPropertiesWidget::exportSVGRequested,
                             view, &DataViewer_Widget::exportToSVG);
            
            // Connect feature add/remove signals from properties to view
            // Use lambdas because std::string parameters don't work directly with Qt signals/slots
            QObject::connect(props, &DataViewerPropertiesWidget::featureAddRequested,
                             view, [view](std::string const & key, std::string const & color) {
                                 view->addFeature(key, color);
                             });
            
            QObject::connect(props, &DataViewerPropertiesWidget::featureRemoveRequested,
                             view, [view](std::string const & key) {
                                 view->removeFeature(key);
                             });
            
            QObject::connect(props, &DataViewerPropertiesWidget::featuresAddRequested,
                             view, [view](std::vector<std::string> const & keys, std::vector<std::string> const & colors) {
                                 view->addFeatures(keys, colors);
                             });
            
            QObject::connect(props, &DataViewerPropertiesWidget::featuresRemoveRequested,
                             view, [view](std::vector<std::string> const & keys) {
                                 view->removeFeatures(keys);
                             });
            
            // Connect color change signal
            QObject::connect(props, &DataViewerPropertiesWidget::featureColorChanged,
                             view, [view](std::string const & key, std::string const & hex_color) {
                                 view->handleColorChanged(key, hex_color);
                             });
            
            // Connect group context menu signal
            QObject::connect(props, &DataViewerPropertiesWidget::groupContextMenuRequested,
                             view, [view](std::string const & group_name, QPoint const & global_pos) {
                                 view->showGroupContextMenu(group_name, global_pos);
                             });

            // Register the state
            reg->registerState(state);

            return EditorRegistry::EditorInstance{
                .state = state,
                .view = view,
                .properties = props
            };
        }
    });

    // Note: Additional data viewer-related types can be registered here in the future
    // For example: DataViewerLite, DataViewerCompare, etc.
}

}  // namespace DataViewerWidgetModule
