#include "MediaLine_Widget.hpp"
#include "ui_MediaLine_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "Media_Window/Media_Window.hpp"
#include "DataManager/transforms/Lines/line_angle.hpp"
#include "DataManager/utils/opencv_utility.hpp"
#include "DataManager/utils/polynomial/polynomial_fit.hpp"
#include "SelectionWidgets/LineNoneSelectionWidget.hpp"
#include "SelectionWidgets/LineAddSelectionWidget.hpp"
#include "SelectionWidgets/LineEraseSelectionWidget.hpp"
#include "SelectionWidgets/LineSelectSelectionWidget.hpp"

#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QButtonGroup>
#include <QCheckBox>
#include <QMenu>
#include <QCursor>
#include <iostream>
#include <armadillo>
#include <opencv2/opencv.hpp>

MediaLine_Widget::MediaLine_Widget(std::shared_ptr<DataManager> data_manager, Media_Window* scene, QWidget* parent)
    : QWidget(parent),
      ui(new Ui::MediaLine_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene} {
    ui->setupUi(this);

    _selection_modes["(None)"] = Selection_Mode::None;
    _selection_modes["Add Points"] = Selection_Mode::Add;
    _selection_modes["Erase Points"] = Selection_Mode::Erase;
    _selection_modes["Select Line"] = Selection_Mode::Select;

    ui->selection_mode_combo->addItems(QStringList(_selection_modes.keys()));
    
    connect(ui->selection_mode_combo, &QComboBox::currentTextChanged, this, &MediaLine_Widget::_toggleSelectionMode);

    connect(ui->color_picker, &ColorPicker_Widget::colorChanged,
            this, &MediaLine_Widget::_setLineColor);
    connect(ui->color_picker, &ColorPicker_Widget::alphaChanged,
            this, &MediaLine_Widget::_setLineAlpha);
    
    // Connect line thickness controls
    connect(ui->line_thickness_slider, &QSlider::valueChanged,
            this, &MediaLine_Widget::_setLineThickness);
    connect(ui->line_thickness_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MediaLine_Widget::_setLineThickness);
    
    // Synchronize line thickness slider and spinbox
    connect(ui->line_thickness_slider, &QSlider::valueChanged,
            ui->line_thickness_spinbox, &QSpinBox::setValue);
    connect(ui->line_thickness_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->line_thickness_slider, &QSlider::setValue);
    
    connect(ui->show_points_checkbox, &QCheckBox::toggled, this, &MediaLine_Widget::_toggleShowPoints);
    
    // Connect position marker controls
    connect(ui->show_position_marker_checkbox, &QCheckBox::toggled, 
            this, &MediaLine_Widget::_toggleShowPositionMarker);
    connect(ui->position_percentage_slider, &QSlider::valueChanged,
            this, &MediaLine_Widget::_setPositionPercentage);
    connect(ui->position_percentage_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MediaLine_Widget::_setPositionPercentage);
    
    // Synchronize position percentage slider and spinbox
    connect(ui->position_percentage_slider, &QSlider::valueChanged,
            ui->position_percentage_spinbox, &QSpinBox::setValue);
    connect(ui->position_percentage_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->position_percentage_slider, &QSlider::setValue);
    
    // Connect segment controls
    connect(ui->show_segment_checkbox, &QCheckBox::toggled, 
            this, &MediaLine_Widget::_toggleShowSegment);
    connect(ui->segment_start_slider, &QSlider::valueChanged,
            this, &MediaLine_Widget::_setSegmentStartPercentage);
    connect(ui->segment_start_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MediaLine_Widget::_setSegmentStartPercentage);
    connect(ui->segment_end_slider, &QSlider::valueChanged,
            this, &MediaLine_Widget::_setSegmentEndPercentage);
    connect(ui->segment_end_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MediaLine_Widget::_setSegmentEndPercentage);
    
    // Synchronize segment slider and spinbox controls
    connect(ui->segment_start_slider, &QSlider::valueChanged,
            ui->segment_start_spinbox, &QSpinBox::setValue);
    connect(ui->segment_start_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->segment_start_slider, &QSlider::setValue);
    connect(ui->segment_end_slider, &QSlider::valueChanged,
            ui->segment_end_spinbox, &QSpinBox::setValue);
    connect(ui->segment_end_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->segment_end_slider, &QSlider::setValue);
    
    connect(ui->line_select_slider, &QSlider::valueChanged, this, &MediaLine_Widget::_lineSelectionChanged);

    _setupSelectionModePages();
}

void MediaLine_Widget::_setupSelectionModePages() {
    _noneSelectionWidget = new line_widget::LineNoneSelectionWidget();
    ui->mode_stacked_widget->addWidget(_noneSelectionWidget);
    
    _addSelectionWidget = new line_widget::LineAddSelectionWidget();
    ui->mode_stacked_widget->addWidget(_addSelectionWidget);
    
    connect(_addSelectionWidget, &line_widget::LineAddSelectionWidget::edgeSnappingToggled,
            this, &MediaLine_Widget::_toggleEdgeSnapping);
    connect(_addSelectionWidget, &line_widget::LineAddSelectionWidget::smoothingModeChanged,
            this, &MediaLine_Widget::_setSmoothingMode);
    connect(_addSelectionWidget, &line_widget::LineAddSelectionWidget::polynomialOrderChanged,
            this, &MediaLine_Widget::_setPolynomialOrder);
    connect(_addSelectionWidget, &line_widget::LineAddSelectionWidget::edgeThresholdChanged,
            this, &MediaLine_Widget::_setEdgeThreshold);
    connect(_addSelectionWidget, &line_widget::LineAddSelectionWidget::edgeSearchRadiusChanged,
            this, &MediaLine_Widget::_setEdgeSearchRadius);
    
    _eraseSelectionWidget = new line_widget::LineEraseSelectionWidget();
    ui->mode_stacked_widget->addWidget(_eraseSelectionWidget);
    
    connect(_eraseSelectionWidget, &line_widget::LineEraseSelectionWidget::eraserRadiusChanged,
            this, &MediaLine_Widget::_setEraserRadius);
    connect(_eraseSelectionWidget, &line_widget::LineEraseSelectionWidget::showCircleToggled,
            this, &MediaLine_Widget::_toggleShowHoverCircle);
    
    _selectSelectionWidget = new line_widget::LineSelectSelectionWidget();
    ui->mode_stacked_widget->addWidget(_selectSelectionWidget);
    
    connect(_selectSelectionWidget, &line_widget::LineSelectSelectionWidget::selectionThresholdChanged,
            this, [this](float threshold) {
                _line_selection_threshold = threshold;
                std::cout << "Line selection threshold set to: " << threshold << std::endl;
            });
    
    ui->mode_stacked_widget->setCurrentIndex(0);
}

MediaLine_Widget::~MediaLine_Widget() {
    delete ui;
}

void MediaLine_Widget::showEvent(QShowEvent * event) {

    static_cast<void>(event);

    std::cout << "Show Event" << std::endl;
    connect(_scene, &Media_Window::leftClickMedia, this, &MediaLine_Widget::_clickedInVideo);
    connect(_scene, &Media_Window::rightClickMedia, this, &MediaLine_Widget::_rightClickedInVideo);
}

void MediaLine_Widget::hideEvent(QHideEvent * event) {

    static_cast<void>(event);

    std::cout << "Hide Event" << std::endl;
    disconnect(_scene, &Media_Window::leftClickMedia, this, &MediaLine_Widget::_clickedInVideo);
    disconnect(_scene, &Media_Window::rightClickMedia, this, &MediaLine_Widget::_rightClickedInVideo);
    
    // Clean up hover circle when switching away from line widget
    _scene->setShowHoverCircle(false);
}

void MediaLine_Widget::setActiveKey(std::string const& key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));

    // Set the color picker to the current line color if available
    if (!key.empty()) {
        auto config = _scene->getLineConfig(key);

        if (config) {
            ui->color_picker->setColor(QString::fromStdString(config.value()->hex_color));
            ui->color_picker->setAlpha(static_cast<int>(config.value()->alpha * 100));
            
            // Set line thickness controls
            ui->line_thickness_slider->blockSignals(true);
            ui->line_thickness_spinbox->blockSignals(true);
            ui->line_thickness_slider->setValue(config.value()->line_thickness);
            ui->line_thickness_spinbox->setValue(config.value()->line_thickness);
            ui->line_thickness_slider->blockSignals(false);
            ui->line_thickness_spinbox->blockSignals(false);
            
            // Update the show points checkbox directly from the UI file
            ui->show_points_checkbox->blockSignals(true);
            ui->show_points_checkbox->setChecked(config.value()->show_points);
            ui->show_points_checkbox->blockSignals(false);
            
            // Set position marker controls
            ui->show_position_marker_checkbox->blockSignals(true);
            ui->show_position_marker_checkbox->setChecked(config.value()->show_position_marker);
            ui->show_position_marker_checkbox->blockSignals(false);
            
            ui->position_percentage_slider->blockSignals(true);
            ui->position_percentage_spinbox->blockSignals(true);
            ui->position_percentage_slider->setValue(config.value()->position_percentage);
            ui->position_percentage_spinbox->setValue(config.value()->position_percentage);
            ui->position_percentage_slider->blockSignals(false);
            ui->position_percentage_spinbox->blockSignals(false);
            
            // Set segment controls
            ui->show_segment_checkbox->blockSignals(true);
            ui->show_segment_checkbox->setChecked(config.value()->show_segment);
            ui->show_segment_checkbox->blockSignals(false);
            
            ui->segment_start_slider->blockSignals(true);
            ui->segment_start_spinbox->blockSignals(true);
            ui->segment_start_slider->setValue(config.value()->segment_start_percentage);
            ui->segment_start_spinbox->setValue(config.value()->segment_start_percentage);
            ui->segment_start_slider->blockSignals(false);
            ui->segment_start_spinbox->blockSignals(false);
            
            ui->segment_end_slider->blockSignals(true);
            ui->segment_end_spinbox->blockSignals(true);
            ui->segment_end_slider->setValue(config.value()->segment_end_percentage);
            ui->segment_end_spinbox->setValue(config.value()->segment_end_percentage);
            ui->segment_end_slider->blockSignals(false);
            ui->segment_end_spinbox->blockSignals(false);
            
            // Reset line selection and update slider
            _current_line_index = 0;
            
            auto line_data = _data_manager->getData<LineData>(_active_key);
            if (line_data) {
                auto current_time = TimeFrameIndex(_data_manager->getCurrentTime());
                auto lines = line_data->getLinesAtTime(current_time);
                
                // Update the slider with the number of lines
                int num_lines = static_cast<int>(lines.size());
                
                ui->line_select_slider->blockSignals(true);
                ui->line_select_slider->setMaximum(num_lines > 0 ? num_lines - 1 : 0);
                ui->line_select_slider->setValue(0); // Reset to first line
                ui->line_select_slider->setEnabled(num_lines > 1);
                ui->line_select_slider->blockSignals(false);
            }
        }
    }
}

void MediaLine_Widget::_setLineAlpha(int alpha) {
    float const alpha_float = static_cast<float>(alpha) / 100;

    if (!_active_key.empty()) {
        auto line_opts = _scene->getLineConfig(_active_key);
        if (line_opts.has_value()) {
            line_opts.value()->alpha = alpha_float;
        }
        _scene->UpdateCanvas();
    }
}

void MediaLine_Widget::_setLineColor(const QString& hex_color) {
    if (!_active_key.empty()) {
        auto line_opts = _scene->getLineConfig(_active_key);
        if (line_opts.has_value()) {
            line_opts.value()->hex_color = hex_color.toStdString();
        }
        _scene->UpdateCanvas();
    }
}

void MediaLine_Widget::_clickedInVideo(qreal x_canvas, qreal y_canvas) {
    if (_active_key.empty()) {
        std::cout << "No active key" << std::endl;
        return;
    }

    auto line_data = _data_manager->getData<LineData>(_active_key);
    if (!line_data) {
        std::cout << "No line data for active key" << std::endl;
        return;
    }

    auto const x_media = static_cast<float>(x_canvas);
    auto const y_media = static_cast<float>(y_canvas);
    auto const current_time = TimeFrameIndex(_data_manager->getCurrentTime());
    auto const line_img_size = line_data->getImageSize();

    auto lines = line_data->getLinesAtTime(current_time);

    switch (_selection_mode) {
        case Selection_Mode::None: {
            std::cout << "Selection mode is None" << std::endl;
            break;
        }
        case Selection_Mode::Add: {
            std::cout << "Selection mode is Add" << std::endl;
            _addPointToLine(x_media, y_media, current_time);
            break;
        }
        case Selection_Mode::Erase: {
            std::cout << "Selection mode is Erase" << std::endl;
            std::cout << "Not yet implemented" << std::endl;
            break;
        }
        case Selection_Mode::Select: {
            std::cout << "Selection mode is Select" << std::endl;
            int nearest_line = _findNearestLine(x_media, y_media);
            if (nearest_line >= 0) {
                _selectLine(nearest_line);
                std::cout << "Selected line " << nearest_line << std::endl;
            } else {
                _clearLineSelection();
                std::cout << "No line found within threshold" << std::endl;
            }
            break;
        }
    }
}

void MediaLine_Widget::_addPointToLine(float x_media, float y_media, TimeFrameIndex current_time) {
    auto line_data = _data_manager->getData<LineData>(_active_key);
    auto lines = line_data->getLinesAtTime(current_time);
    
    // Check if edge snapping is enabled
    bool use_edge_snapping = false;
    auto line_opts = _scene->getLineConfig(_active_key);
    if (line_opts.has_value()) {
        use_edge_snapping = line_opts.value()->edge_snapping;
    }
    
    if (use_edge_snapping && _edge_snapping_enabled) {

        if (_current_edges.empty()) {
            _detectEdges();
        }
        
        auto edge_point = _findNearestEdge(x_media, y_media);
        x_media = edge_point.first;
        y_media = edge_point.second;
    }
    
    if (lines.empty()) {
        // If no lines exist, create a new one with the single point
        _data_manager->getData<LineData>(_active_key)->addLineAtTime(current_time, {{x_media, y_media}});
        // After adding a new line, it's line index 0
        _current_line_index = 0;
        ui->line_select_slider->setValue(0);
    } else {
        if (_smoothing_mode == Smoothing_Mode::SimpleSmooth) {
            // Use the original smoothing approach with the selected line index
            _data_manager->getData<LineData>(_active_key)->addPointToLineInterpolate(
                current_time, _current_line_index, Point2D<float>{x_media, y_media});
        } else if (_smoothing_mode == Smoothing_Mode::PolynomialFit) {
            // Make sure current_line_index is valid
            if (_current_line_index >= static_cast<int>(lines.size())) {
                std::cout << "Warning: line index out of bounds, using first line" << std::endl;
                _current_line_index = 0;
                ui->line_select_slider->setValue(0);
            }
            
            // Get a copy of the current line using the selected index
            auto line = lines[_current_line_index];
            
            // If the line already exists, add interpolated points between the last point and the new point
            if (!line.empty()) {
                Point2D<float> const last_point = line.back();
                
                // Calculate distance between last point and new point
                float dx = x_media - last_point.x;
                float dy = y_media - last_point.y;
                float distance = std::sqrt(dx*dx + dy*dy);
                
                // Add interpolated points if the distance is significant
                if (distance > 5.0f) {  // Threshold for adding interpolation
                    int num_interp_points = std::max(2, static_cast<int>(distance / 5.0f));
                    for (int i = 1; i <= num_interp_points; ++i) {
                        float t = static_cast<float>(i) / (num_interp_points + 1);
                        float interp_x = last_point.x + t * dx;
                        float interp_y = last_point.y + t * dy;
                        line.push_back(Point2D<float>{interp_x, interp_y});
                    }
                }
            }
            
            // Add the actual new point
            line.push_back(Point2D<float>{x_media, y_media});
            
            // Apply polynomial fitting if we have enough points
            if (line.size() >= 3) {
                _applyPolynomialFit(line, _polynomial_order);
            }
            
            // Update the line in the data manager
            std::vector<Line2D> updated_lines = lines;
            updated_lines[_current_line_index] = line;
            
            _data_manager->getData<LineData>(_active_key)->clearAtTime(current_time);
            for(const auto& updated_line : updated_lines) {
                _data_manager->getData<LineData>(_active_key)->addLineAtTime(current_time, updated_line);
            }
        }
    }

    _scene->UpdateCanvas();
    std::cout << "Added point (" << x_media << ", " << y_media << ") to line " 
              << _active_key << " (index: " << _current_line_index << ")" << std::endl;
}

void MediaLine_Widget::_applyPolynomialFit(Line2D& line, int order) {
    if (line.size() < static_cast<size_t>(order + 1)) {
        // Not enough points for the requested polynomial order
        return;
    }
    
    // Extract x and y coordinates
    std::vector<double> t(line.size());
    std::vector<double> x_coords(line.size());
    std::vector<double> y_coords(line.size());
    
    // Use parameter t along the curve (0 to 1)
    for (size_t i = 0; i < line.size(); ++i) {
        t[i] = static_cast<double>(i) / (line.size() - 1);
        x_coords[i] = line[i].x;
        y_coords[i] = line[i].y;
    }
    
    // Fit polynomials to x(t) and y(t) using the function from line_angle.hpp
    std::vector<double> x_coeffs = fit_polynomial(t, x_coords, order);
    std::vector<double> y_coeffs = fit_polynomial(t, y_coords, order);
    
    if (x_coeffs.empty() || y_coeffs.empty()) {
        // Fall back to simple smoothing if fitting failed
        smooth_line(line);
        return;
    }
    
    // Generate smooth curve with more points
    int const num_points = std::max(100, static_cast<int>(line.size()) * 2);
    Line2D smooth_line;
    smooth_line.reserve(num_points);
    
    for (int i = 0; i < num_points; ++i) {
        double t_param = static_cast<double>(i) / (num_points - 1);
        
        // Evaluate polynomials at t_param using the function from line_angle.hpp
        double x_val = evaluate_polynomial(x_coeffs, t_param);
        double y_val = evaluate_polynomial(y_coeffs, t_param);
        
        smooth_line.push_back(Point2D<float>{static_cast<float>(x_val), static_cast<float>(y_val)});
    }
    
    // Replace the original line with the smooth one
    line = std::move(smooth_line);
}

void MediaLine_Widget::_setSmoothingMode(int index) {
    _smoothing_mode = static_cast<Smoothing_Mode>(index);
    std::cout << "Smoothing mode set to: " << index << std::endl;
}

void MediaLine_Widget::_setPolynomialOrder(int order) {
    _polynomial_order = order;
    std::cout << "Polynomial order set to: " << order << std::endl;
}

void MediaLine_Widget::_toggleSelectionMode(QString text) {
    _selection_mode = _selection_modes[text];
    
    // Switch to the appropriate page in the stacked widget
    int pageIndex = static_cast<int>(_selection_mode);
    ui->mode_stacked_widget->setCurrentIndex(pageIndex);

    if (_selection_mode == Selection_Mode::Erase) {
        _scene->setShowHoverCircle(true);
        _scene->setHoverCircleRadius(_eraseSelectionWidget->getEraserRadius());
    } else {
        _scene->setShowHoverCircle(false);
    }
}

void MediaLine_Widget::_toggleShowPoints(bool checked) {
    if (!_active_key.empty()) {
        auto line_opts = _scene->getLineConfig(_active_key);
        if (line_opts.has_value()) {
            line_opts.value()->show_points = checked;
        }
        _scene->UpdateCanvas();
    }
}

void MediaLine_Widget::_toggleEdgeSnapping(bool checked) {
    _edge_snapping_enabled = checked;
    
    if (!_active_key.empty()) {
        auto line_opts = _scene->getLineConfig(_active_key);
        if (line_opts.has_value()) {
            line_opts.value()->edge_snapping = checked;
        }
        
        // If enabling edge snapping, perform edge detection immediately
        if (checked) {
            _detectEdges();
        } else {
            // Clear cached edges when disabling
            _current_edges.release();
        }
        
        _scene->UpdateCanvas();
    }
    
    std::cout << "Edge snapping " << (checked ? "enabled" : "disabled") << std::endl;
}

void MediaLine_Widget::LoadFrame(int frame_id) {
    // Update the widget with the new frame
    // This could involve refreshing displays or updating UI elements
    // specific to the current frame
    
    // If we have an active line, we might want to update some UI
    // based on line data at this frame
    if (!_active_key.empty()) {
        auto line_data = _data_manager->getData<LineData>(_active_key);
        if (line_data) {
            auto lines = line_data->getLinesAtTime(TimeFrameIndex(frame_id));
            
            // Update the line_select_slider's maximum value based on the number of lines
            int num_lines = static_cast<int>(lines.size());
            
            // Disconnect and reconnect to avoid triggering slider value changed signals
            // during this programmatic update
            ui->line_select_slider->blockSignals(true);
            
            // Set the maximum value to the number of lines - 1 (or 0 if there are no lines)
            // Slider indices are 0-based, so max should be (num_lines - 1) when there are lines
            ui->line_select_slider->setMaximum(num_lines > 0 ? num_lines - 1 : 0);
            
            // If the current slider value exceeds the new maximum, adjust it
            if (ui->line_select_slider->value() > ui->line_select_slider->maximum()) {
                ui->line_select_slider->setValue(ui->line_select_slider->maximum());
            }
            
            ui->line_select_slider->blockSignals(false);
            
            // Update slider enabled state
            ui->line_select_slider->setEnabled(num_lines > 1);
            
            std::cout << "Frame " << frame_id << ": Updated line selector for " 
                      << num_lines << " lines in " << _active_key << std::endl;
        }
    }
}

void MediaLine_Widget::_lineSelectionChanged(int index) {
    if (_current_line_index == index) {
        return; // No change
    }
    
    _current_line_index = index;
    std::cout << "Selected line index: " << _current_line_index << std::endl;
    
    // Update any UI or visualization based on the selected line
    if (!_active_key.empty()) {
        auto line_data = _data_manager->getData<LineData>(_active_key);
        if (line_data) {
            auto current_time = TimeFrameIndex(_data_manager->getCurrentTime());
            auto lines = line_data->getLinesAtTime(current_time);
            
            if (!lines.empty() && _current_line_index < static_cast<int>(lines.size())) {
                // Here you can perform any specific actions needed when a different line is selected
                // For example, updating a visualization to highlight the selected line
                
                // Request canvas update to reflect any visualization changes
                _scene->UpdateCanvas();
            }
        }
    }
}

void MediaLine_Widget::_setEdgeThreshold(int threshold) {
    _edge_threshold = threshold;
    std::cout << "Edge threshold set to: " << threshold << std::endl;
}

void MediaLine_Widget::_setEdgeSearchRadius(int radius) {
    _edge_search_radius = radius;
    std::cout << "Edge search radius set to: " << radius << std::endl;
}

void MediaLine_Widget::_detectEdges() {

    auto media = _data_manager->getData<MediaData>("media");
    if (!media) {
        std::cout << "No media data available for edge detection" << std::endl;
        return;
    }
    
    auto const current_time = _data_manager->getCurrentTime();
    auto frame_data = media->getProcessedData(current_time);
    
    // Convert raw frame data to cv::Mat
    /*
    int width = media->getWidth();
    int height = media->getHeight();
    
    // Create a grayscale image for edge detection
    cv::Mat gray_image;
    
    // Check the format of media data and create appropriate cv::Mat
    if (media->getFormat() == MediaData::DisplayFormat::Gray) {
        // Grayscale image
        gray_image = cv::Mat(height, width, CV_8UC1, frame_data.data());
    } else {
        // Color image (RGBA)
        _current_frame = cv::Mat(height, width, CV_8UC4, frame_data.data());
        
        // Convert to grayscale
        cv::cvtColor(_current_frame, gray_image, cv::COLOR_RGBA2GRAY);
    }
    */
    auto gray_image = convert_vector_to_mat(frame_data, media->getImageSize());
    
    //cv::Mat blurred;
    //cv::GaussianBlur(gray_image, blurred, cv::Size(5, 5), 1.5);
    //cv::Canny(blurred, _current_edges, _edge_threshold / 2, _edge_threshold);

    cv::Canny(gray_image, _current_edges, _edge_threshold / 2, _edge_threshold);
    
    std::cout << "Edge detection completed for frame " << current_time << std::endl;
    std::cout << "Edges detected: " << _current_edges.size() << std::endl;
}

std::pair<float, float> MediaLine_Widget::_findNearestEdge(float x, float y) {
    if (_current_edges.empty()) {
        return {x, y};
    }
    
    // Round x and y to integers (image coordinates)
    int x_int = static_cast<int>(std::round(x));
    int y_int = static_cast<int>(std::round(y));
    
    // Define search radius and initialize variables
    int radius = _edge_search_radius;
    float min_distance = radius * radius + 1; // Initialize to something larger than possible
    std::pair<float, float> nearest_edge = {x, y}; // Default to original point
    
    // Get image dimensions
    int width = _current_edges.cols;
    int height = _current_edges.rows;
    
    // Check if the point is within image bounds
    if (x_int < 0 || x_int >= width || y_int < 0 || y_int >= height) {
        std::cout << "Click point outside image bounds" << std::endl;
        return {x, y};
    }
    
    // Search in a square region around the clicked point
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            // Calculate current point to check
            int nx = x_int + dx;
            int ny = y_int + dy;
            
            if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                continue;
            }
            
            // Check if this is an edge pixel
            if (_current_edges.at<uchar>(ny, nx) > 0) {
                // Calculate distance squared (avoid square root for performance)
                float d_squared = dx*dx + dy*dy;
                
                // Update nearest edge if this is closer
                if (d_squared < min_distance) {
                    min_distance = d_squared;
                    nearest_edge = {static_cast<float>(nx), static_cast<float>(ny)};
                }
            }
        }
    }
    
    if (min_distance < radius * radius + 1) {
        std::cout << "Found edge point at (" << nearest_edge.first << ", " 
                  << nearest_edge.second << "), distance: " << std::sqrt(min_distance) << std::endl;
    } else {
        std::cout << "No edge found within radius " << radius << std::endl;
        cv::imwrite("edges.png", _current_edges);
    }
    
    return nearest_edge;
}

void MediaLine_Widget::_setEraserRadius(int radius) {
    if (_selection_mode == Selection_Mode::Erase) {
        _scene->setHoverCircleRadius(static_cast<double>(radius));
    }
    std::cout << "Eraser radius set to: " << radius << std::endl;
}

void MediaLine_Widget::_toggleShowHoverCircle(bool checked) {
    _scene->setShowHoverCircle(checked);
    std::cout << "Show hover circle " << (checked ? "enabled" : "disabled") << std::endl;
}

void MediaLine_Widget::_setLineThickness(int thickness) {
    if (!_active_key.empty()) {
        auto line_opts = _scene->getLineConfig(_active_key);
        if (line_opts.has_value()) {
            line_opts.value()->line_thickness = thickness;
        }
        _scene->UpdateCanvas();
    }
    
    // Synchronize slider and spinbox if the signal came from one of them
    QObject* sender_obj = sender();
    if (sender_obj == ui->line_thickness_slider) {
        ui->line_thickness_spinbox->blockSignals(true);
        ui->line_thickness_spinbox->setValue(thickness);
        ui->line_thickness_spinbox->blockSignals(false);
    } else if (sender_obj == ui->line_thickness_spinbox) {
        ui->line_thickness_slider->blockSignals(true);
        ui->line_thickness_slider->setValue(thickness);
        ui->line_thickness_slider->blockSignals(false);
    }
    
    std::cout << "Line thickness set to: " << thickness << std::endl;
}

void MediaLine_Widget::_toggleShowPositionMarker(bool checked) {
    if (!_active_key.empty()) {
        auto line_opts = _scene->getLineConfig(_active_key);
        if (line_opts.has_value()) {
            line_opts.value()->show_position_marker = checked;
        }
        _scene->UpdateCanvas();
    }
    std::cout << "Show position marker " << (checked ? "enabled" : "disabled") << std::endl;
}

void MediaLine_Widget::_setPositionPercentage(int percentage) {
    if (!_active_key.empty()) {
        auto line_opts = _scene->getLineConfig(_active_key);
        if (line_opts.has_value()) {
            line_opts.value()->position_percentage = percentage;
        }
        _scene->UpdateCanvas();
    }
    
    // Synchronize slider and spinbox if the signal came from one of them
    QObject* sender_obj = sender();
    if (sender_obj == ui->position_percentage_slider) {
        ui->position_percentage_spinbox->blockSignals(true);
        ui->position_percentage_spinbox->setValue(percentage);
        ui->position_percentage_spinbox->blockSignals(false);
    } else if (sender_obj == ui->position_percentage_spinbox) {
        ui->position_percentage_slider->blockSignals(true);
        ui->position_percentage_slider->setValue(percentage);
        ui->position_percentage_slider->blockSignals(false);
    }
    
    std::cout << "Position percentage set to: " << percentage << std::endl;
}

void MediaLine_Widget::_toggleShowSegment(bool checked) {
    if (!_active_key.empty()) {
        auto line_opts = _scene->getLineConfig(_active_key);
        if (line_opts.has_value()) {
            line_opts.value()->show_segment = checked;
        }
        _scene->UpdateCanvas();
    }
    std::cout << "Show segment " << (checked ? "enabled" : "disabled") << std::endl;
}

void MediaLine_Widget::_setSegmentStartPercentage(int percentage) {
    if (!_active_key.empty()) {
        auto line_opts = _scene->getLineConfig(_active_key);
        if (line_opts.has_value()) {
            // Ensure start percentage doesn't exceed end percentage
            if (percentage > line_opts.value()->segment_end_percentage) {
                // Don't allow start to exceed end
                QObject* sender_obj = sender();
                if (sender_obj == ui->segment_start_slider) {
                    ui->segment_start_slider->blockSignals(true);
                    ui->segment_start_slider->setValue(line_opts.value()->segment_end_percentage);
                    ui->segment_start_slider->blockSignals(false);
                } else if (sender_obj == ui->segment_start_spinbox) {
                    ui->segment_start_spinbox->blockSignals(true);
                    ui->segment_start_spinbox->setValue(line_opts.value()->segment_end_percentage);
                    ui->segment_start_spinbox->blockSignals(false);
                }
                percentage = line_opts.value()->segment_end_percentage;
            }
            
            line_opts.value()->segment_start_percentage = percentage;
        }
        _scene->UpdateCanvas();
    }
    
    // Synchronize slider and spinbox if the signal came from one of them
    QObject* sender_obj = sender();
    if (sender_obj == ui->segment_start_slider) {
        ui->segment_start_spinbox->blockSignals(true);
        ui->segment_start_spinbox->setValue(percentage);
        ui->segment_start_spinbox->blockSignals(false);
    } else if (sender_obj == ui->segment_start_spinbox) {
        ui->segment_start_slider->blockSignals(true);
        ui->segment_start_slider->setValue(percentage);
        ui->segment_start_slider->blockSignals(false);
    }
    
    std::cout << "Segment start percentage set to: " << percentage << std::endl;
}

void MediaLine_Widget::_setSegmentEndPercentage(int percentage) {
    if (!_active_key.empty()) {
        auto line_opts = _scene->getLineConfig(_active_key);
        if (line_opts.has_value()) {
            // Ensure end percentage doesn't go below start percentage
            if (percentage < line_opts.value()->segment_start_percentage) {
                // Don't allow end to go below start
                QObject* sender_obj = sender();
                if (sender_obj == ui->segment_end_slider) {
                    ui->segment_end_slider->blockSignals(true);
                    ui->segment_end_slider->setValue(line_opts.value()->segment_start_percentage);
                    ui->segment_end_slider->blockSignals(false);
                } else if (sender_obj == ui->segment_end_spinbox) {
                    ui->segment_end_spinbox->blockSignals(true);
                    ui->segment_end_spinbox->setValue(line_opts.value()->segment_start_percentage);
                    ui->segment_end_spinbox->blockSignals(false);
                }
                percentage = line_opts.value()->segment_start_percentage;
            }
            
            line_opts.value()->segment_end_percentage = percentage;
        }
        _scene->UpdateCanvas();
    }
    
    // Synchronize slider and spinbox if the signal came from one of them
    QObject* sender_obj = sender();
    if (sender_obj == ui->segment_end_slider) {
        ui->segment_end_spinbox->blockSignals(true);
        ui->segment_end_spinbox->setValue(percentage);
        ui->segment_end_spinbox->blockSignals(false);
    } else if (sender_obj == ui->segment_end_spinbox) {
        ui->segment_end_slider->blockSignals(true);
        ui->segment_end_slider->setValue(percentage);
        ui->segment_end_slider->blockSignals(false);
    }
    
    std::cout << "Segment end percentage set to: " << percentage << std::endl;
}

void MediaLine_Widget::_rightClickedInVideo(qreal x_canvas, qreal y_canvas) {
    // Only handle right-clicks in Select mode and when a line is selected
    if (_selection_mode != Selection_Mode::Select || _selected_line_index < 0 || _active_key.empty()) {
        return;
    }
    
    auto x_media = static_cast<float>(x_canvas);
    auto y_media = static_cast<float>(y_canvas);
    
    // Check if the right-click is near the selected line
    int nearest_line = _findNearestLine(x_media, y_media);
    if (nearest_line == _selected_line_index) {
        // Show context menu at the click position
        QPoint global_pos = QCursor::pos();
        _showLineContextMenu(global_pos);
    }
}

int MediaLine_Widget::_findNearestLine(float x, float y) {
    if (_active_key.empty()) {
        return -1;
    }
    
    auto line_data = _data_manager->getData<LineData>(_active_key);
    if (!line_data) {
        return -1;
    }
    
    auto current_time = TimeFrameIndex(_data_manager->getCurrentTime());
    auto lines = line_data->getLinesAtTime(current_time);
    
    if (lines.empty()) {
        return -1;
    }
    
    int nearest_line_index = -1;
    float min_distance = _line_selection_threshold + 1; // Initialize beyond threshold
    
    for (int line_idx = 0; line_idx < static_cast<int>(lines.size()); ++line_idx) {
        const auto& line = lines[line_idx];
        
        if (line.empty()) {
            continue;
        }
        
        // Check distance to each point on the line
        for (const auto& point : line) {
            float dx = x - point.x;
            float dy = y - point.y;
            float distance = std::sqrt(dx*dx + dy*dy);
            
            if (distance < min_distance) {
                min_distance = distance;
                nearest_line_index = line_idx;
            }
        }
    }
    
    return (min_distance <= _line_selection_threshold) ? nearest_line_index : -1;
}

void MediaLine_Widget::_selectLine(int line_index) {
    _selected_line_index = line_index;
    
    // Update the line display options to show the selected line differently
    if (!_active_key.empty()) {
        auto line_opts = _scene->getLineConfig(_active_key);
        if (line_opts.has_value()) {
            line_opts.value()->selected_line_index = line_index;
        }
        _scene->UpdateCanvas();
    }
}

void MediaLine_Widget::_clearLineSelection() {
    _selected_line_index = -1;
    
    // Update the line display options to clear selection
    if (!_active_key.empty()) {
        auto line_opts = _scene->getLineConfig(_active_key);
        if (line_opts.has_value()) {
            line_opts.value()->selected_line_index = -1;
        }
        _scene->UpdateCanvas();
    }
}

void MediaLine_Widget::_showLineContextMenu(const QPoint& position) {
    QMenu context_menu(this);
    
    // Create Move To submenu
    QMenu* move_menu = context_menu.addMenu("Move Line To");
    QMenu* copy_menu = context_menu.addMenu("Copy Line To");
    
    // Get available LineData keys
    auto available_keys = _getAvailableLineDataKeys();
    
    for (const auto& key : available_keys) {
        if (key != _active_key) { // Don't include the current key
            // Add to Move menu
            QAction* move_action = move_menu->addAction(QString::fromStdString(key));
            connect(move_action, &QAction::triggered, [this, key]() {
                _moveLineToTarget(key);
            });
            
            // Add to Copy menu
            QAction* copy_action = copy_menu->addAction(QString::fromStdString(key));
            connect(copy_action, &QAction::triggered, [this, key]() {
                _copyLineToTarget(key);
            });
        }
    }
    
    // Disable menus if no other LineData available
    if (available_keys.size() <= 1) {
        move_menu->setEnabled(false);
        copy_menu->setEnabled(false);
    }
    
    context_menu.exec(position);
}

std::vector<std::string> MediaLine_Widget::_getAvailableLineDataKeys() {
    return _data_manager->getKeys<LineData>();
}

void MediaLine_Widget::_moveLineToTarget(const std::string& target_key) {
    if (_selected_line_index < 0 || _active_key.empty()) {
        return;
    }
    
    auto source_line_data = _data_manager->getData<LineData>(_active_key);
    auto target_line_data = _data_manager->getData<LineData>(target_key);
    
    if (!source_line_data || !target_line_data) {
        std::cerr << "Could not retrieve source or target LineData" << std::endl;
        return;
    }
    
    auto current_time = TimeFrameIndex(_data_manager->getCurrentTime());
    auto lines = source_line_data->getLinesAtTime(current_time);
    
    if (_selected_line_index >= static_cast<int>(lines.size())) {
        std::cerr << "Selected line index out of bounds" << std::endl;
        return;
    }
    
    // Get the selected line
    Line2D selected_line = lines[_selected_line_index];
    
    // Add to target
    target_line_data->addLineAtTime(current_time, selected_line);
    
    // Remove from source by rebuilding the vector without the selected line
    std::vector<Line2D> remaining_lines;
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        if (i != _selected_line_index) {
            remaining_lines.push_back(lines[i]);
        }
    }

    // Clear and rebuild source lines
    source_line_data->clearAtTime(TimeFrameIndex(current_time));
    for (const auto& line : remaining_lines) {
        source_line_data->addLineAtTime(current_time, line);
    }
    
    // Clear selection since the line was moved
    _clearLineSelection();
    
    std::cout << "Moved line from " << _active_key << " to " << target_key << std::endl;
}

void MediaLine_Widget::_copyLineToTarget(const std::string& target_key) {
    if (_selected_line_index < 0 || _active_key.empty()) {
        return;
    }
    
    auto source_line_data = _data_manager->getData<LineData>(_active_key);
    auto target_line_data = _data_manager->getData<LineData>(target_key);
    
    if (!source_line_data || !target_line_data) {
        std::cerr << "Could not retrieve source or target LineData" << std::endl;
        return;
    }
    
    auto current_time = TimeFrameIndex(_data_manager->getCurrentTime());
    auto lines = source_line_data->getLinesAtTime(current_time);
    
    if (_selected_line_index >= static_cast<int>(lines.size())) {
        std::cerr << "Selected line index out of bounds" << std::endl;
        return;
    }
    
    // Get the selected line and copy it to target
    Line2D selected_line = lines[_selected_line_index];
    target_line_data->addLineAtTime(current_time, selected_line);
    
    std::cout << "Copied line from " << _active_key << " to " << target_key << std::endl;
}
