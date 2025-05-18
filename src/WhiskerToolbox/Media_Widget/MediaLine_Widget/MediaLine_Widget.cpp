#include "MediaLine_Widget.hpp"
#include "ui_MediaLine_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "Media_Window/Media_Window.hpp"

#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QButtonGroup>
#include <iostream>
#include <armadillo>

// Helper function to fit a polynomial of the specified order to the given data
// Copied from line_angle.cpp to reuse the armadillo implementation
std::vector<double> fit_polynomial(std::vector<double> const &x, std::vector<double> const &y, int order) {
    if (x.size() != y.size() || x.size() <= order) {
        return {};  // Not enough data points or size mismatch
    }

    // Create Armadillo matrix for Vandermonde matrix
    arma::mat X(x.size(), order + 1);
    arma::vec Y(y.data(), y.size());

    // Build Vandermonde matrix
    for (size_t i = 0; i < x.size(); ++i) {
        for (int j = 0; j <= order; ++j) {
            X(i, j) = std::pow(x[i], j);
        }
    }

    // Solve least squares problem: X * coeffs = Y
    arma::vec coeffs;
    bool success = arma::solve(coeffs, X, Y);
    
    if (!success) {
        return {}; // Failed to solve
    }

    // Convert Armadillo vector to std::vector
    std::vector<double> result(coeffs.begin(), coeffs.end());
    return result;
}

// Helper function to evaluate polynomial at a given point
double evaluate_polynomial(std::vector<double> const &coeffs, double x) {
    double result = 0.0;
    for (size_t i = 0; i < coeffs.size(); ++i) {
        result += coeffs[i] * std::pow(x, i);
    }
    return result;
}

MediaLine_Widget::MediaLine_Widget(std::shared_ptr<DataManager> data_manager, Media_Window* scene, QWidget* parent)
    : QWidget(parent),
      ui(new Ui::MediaLine_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene} {
    ui->setupUi(this);

    _selection_modes["(None)"] = Selection_Mode::None;
    _selection_modes["Add Points"] = Selection_Mode::Add;
    _selection_modes["Erase Points"] = Selection_Mode::Erase;

    ui->selection_mode_combo->addItems(QStringList(_selection_modes.keys()));
    
    connect(ui->selection_mode_combo, &QComboBox::currentTextChanged, this, &MediaLine_Widget::_toggleSelectionMode);

    connect(ui->color_picker, &ColorPicker_Widget::colorChanged,
            this, &MediaLine_Widget::_setLineColor);
    connect(ui->color_picker, &ColorPicker_Widget::alphaChanged,
            this, &MediaLine_Widget::_setLineAlpha);
            
    // Create the UI pages for each selection mode
    _setupSelectionModePages();
}

void MediaLine_Widget::_setupSelectionModePages() {
    // Create the "None" mode page
    QWidget* nonePage = new QWidget();
    QVBoxLayout* noneLayout = new QVBoxLayout(nonePage);
    QLabel* noneLabel = new QLabel("No selection mode active. Click in the video to navigate.");
    noneLabel->setWordWrap(true);
    noneLayout->addWidget(noneLabel);
    ui->mode_stacked_widget->addWidget(nonePage);
    
    // Create the "Add Points" mode page with enhanced options
    QWidget* addPage = new QWidget();
    QVBoxLayout* addLayout = new QVBoxLayout(addPage);
    
    // Description
    QLabel* addLabel = new QLabel("Add points mode: Click in the video to add points to the end of the selected line.");
    addLabel->setWordWrap(true);
    addLayout->addWidget(addLabel);
    
    // Smoothing options group
    QGroupBox* smoothingGroupBox = new QGroupBox("Smoothing Method");
    QVBoxLayout* smoothingLayout = new QVBoxLayout(smoothingGroupBox);
    
    // Radio buttons for smoothing options
    QButtonGroup* smoothingGroup = new QButtonGroup(addPage);
    QRadioButton* simpleSmooth = new QRadioButton("Simple Smoothing");
    QRadioButton* polyFit = new QRadioButton("Polynomial Fit");
    simpleSmooth->setChecked(true); // Default
    
    smoothingGroup->addButton(simpleSmooth, static_cast<int>(Smoothing_Mode::SimpleSmooth));
    smoothingGroup->addButton(polyFit, static_cast<int>(Smoothing_Mode::PolynomialFit));
    
    smoothingLayout->addWidget(simpleSmooth);
    smoothingLayout->addWidget(polyFit);
    
    // Polynomial order spinner
    QHBoxLayout* polyOrderLayout = new QHBoxLayout();
    QLabel* polyOrderLabel = new QLabel("Polynomial Order:");
    QSpinBox* polyOrderSpinBox = new QSpinBox();
    polyOrderSpinBox->setRange(1, 10);
    polyOrderSpinBox->setValue(_polynomial_order);
    polyOrderSpinBox->setEnabled(false); // Disabled initially since simple smoothing is selected
    
    polyOrderLayout->addWidget(polyOrderLabel);
    polyOrderLayout->addWidget(polyOrderSpinBox);
    smoothingLayout->addLayout(polyOrderLayout);
    
    addLayout->addWidget(smoothingGroupBox);
    ui->mode_stacked_widget->addWidget(addPage);
    
    // Connect signals for the smoothing options
    connect(smoothingGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &MediaLine_Widget::_setSmoothingMode);
    connect(polyOrderSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MediaLine_Widget::_setPolynomialOrder);
            
    // Enable/disable polynomial order based on selected smoothing mode
    connect(polyFit, &QRadioButton::toggled, polyOrderSpinBox, &QSpinBox::setEnabled);
    
    // Create the "Erase Points" mode page
    QWidget* erasePage = new QWidget();
    QVBoxLayout* eraseLayout = new QVBoxLayout(erasePage);
    QLabel* eraseLabel = new QLabel("Erase points mode: Click in the video to remove points around the mouse click location.");
    eraseLabel->setWordWrap(true);
    eraseLayout->addWidget(eraseLabel);
    ui->mode_stacked_widget->addWidget(erasePage);
    
    // Set initial page
    ui->mode_stacked_widget->setCurrentIndex(0);
}

MediaLine_Widget::~MediaLine_Widget() {
    delete ui;
}

void MediaLine_Widget::showEvent(QShowEvent * event) {
    std::cout << "Show Event" << std::endl;
   connect(_scene, &Media_Window::leftClickMedia, this, &MediaLine_Widget::_clickedInVideo);

}

void MediaLine_Widget::hideEvent(QHideEvent * event) {
    std::cout << "Hide Event" << std::endl;
    disconnect(_scene, &Media_Window::leftClickMedia, this, &MediaLine_Widget::_clickedInVideo);
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
    auto const current_time = _data_manager->getTime()->getLastLoadedFrame();
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
    }
}

void MediaLine_Widget::_addPointToLine(float x_media, float y_media, int current_time) {
    auto line_data = _data_manager->getData<LineData>(_active_key);
    auto lines = line_data->getLinesAtTime(current_time);
    
    if (lines.empty()) {
        // If no line exists, create a new one with the single point
        _data_manager->getData<LineData>(_active_key)->addLineAtTime(current_time, {{x_media, y_media}});
    } else {
        if (_smoothing_mode == Smoothing_Mode::SimpleSmooth) {
            // Use the original smoothing approach
            _data_manager->getData<LineData>(_active_key)->addPointToLineInterpolate(current_time, 0, Point2D<float>{x_media, y_media});
        } else if (_smoothing_mode == Smoothing_Mode::PolynomialFit) {
            // Get a copy of the current line
            auto line = lines[0];
            
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
            _data_manager->getData<LineData>(_active_key)->clearLinesAtTime(current_time);
            _data_manager->getData<LineData>(_active_key)->addLineAtTime(current_time, line);
        }
    }

    _scene->UpdateCanvas();
    std::cout << "Added point (" << x_media << ", " << y_media << ") to line " << _active_key << std::endl;
}

void MediaLine_Widget::_applyPolynomialFit(Line2D& line, int order) {
    if (line.size() < order + 1) {
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
    
    // Fit polynomials to x(t) and y(t) using the existing function
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
        
        // Evaluate polynomials at t_param using our helper function
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
        _scene->setHoverCircleRadius(10.0);
    } else {
        _scene->setShowHoverCircle(false);
    }
}

/*
void MediaLine_Widget::_clearCurrentLine() {
    if (_active_key.empty()) {
        return;
    }

    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    if (_data_manager->getData<LineData>(_active_key)) {
        _data_manager->getData<LineData>(_active_key)->clearLinesAtTime(current_time);
        _scene->UpdateCanvas();
        std::cout << "Cleared line " << _active_key << " at time " << current_time << std::endl;
    }
}
*/
