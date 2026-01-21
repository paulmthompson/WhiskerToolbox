#include "TestWidgetProperties.hpp"
#include "TestWidgetState.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QColorDialog>

TestWidgetProperties::TestWidgetProperties(std::shared_ptr<TestWidgetState> state,
                                           QWidget * parent)
    : QWidget(parent)
    , _state(std::move(state)) {
    
    setupUi();
    connectUiSignals();
    connectStateSignals();
}

void TestWidgetProperties::setupUi() {
    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(8, 8, 8, 8);
    main_layout->setSpacing(12);

    // === Title ===
    auto * title_label = new QLabel("<b>Test Widget Properties</b>");
    title_label->setAlignment(Qt::AlignCenter);
    main_layout->addWidget(title_label);

    // === Feature Toggles Group ===
    auto * toggles_group = new QGroupBox("Display Options");
    auto * toggles_layout = new QVBoxLayout(toggles_group);

    _show_grid_checkbox = new QCheckBox("Show Grid");
    _show_grid_checkbox->setChecked(_state ? _state->showGrid() : true);
    toggles_layout->addWidget(_show_grid_checkbox);

    _show_crosshair_checkbox = new QCheckBox("Show Crosshair");
    _show_crosshair_checkbox->setChecked(_state ? _state->showCrosshair() : false);
    toggles_layout->addWidget(_show_crosshair_checkbox);

    _enable_animation_checkbox = new QCheckBox("Enable Animation");
    _enable_animation_checkbox->setChecked(_state ? _state->enableAnimation() : false);
    toggles_layout->addWidget(_enable_animation_checkbox);

    main_layout->addWidget(toggles_group);

    // === Color Group ===
    auto * color_group = new QGroupBox("Appearance");
    auto * color_layout = new QFormLayout(color_group);

    _color_button = new QPushButton();
    _color_button->setFixedSize(60, 24);
    updateColorButtonStyle();
    color_layout->addRow("Highlight Color:", _color_button);

    main_layout->addWidget(color_group);

    // === Numeric Controls Group ===
    auto * numeric_group = new QGroupBox("View Settings");
    auto * numeric_layout = new QFormLayout(numeric_group);

    // Zoom slider with label
    auto * zoom_container = new QWidget();
    auto * zoom_hbox = new QHBoxLayout(zoom_container);
    zoom_hbox->setContentsMargins(0, 0, 0, 0);

    _zoom_slider = new QSlider(Qt::Horizontal);
    _zoom_slider->setRange(10, 500);  // 0.1 to 5.0 scaled by 100
    int const initial_zoom = _state ? static_cast<int>(_state->zoomLevel() * 100) : 100;
    _zoom_slider->setValue(initial_zoom);
    zoom_hbox->addWidget(_zoom_slider, 1);

    _zoom_label = new QLabel(QString::number(initial_zoom / 100.0, 'f', 1) + "x");
    _zoom_label->setFixedWidth(40);
    zoom_hbox->addWidget(_zoom_label);

    numeric_layout->addRow("Zoom:", zoom_container);

    // Grid spacing spinbox
    _grid_spacing_spinbox = new QSpinBox();
    _grid_spacing_spinbox->setRange(10, 200);
    _grid_spacing_spinbox->setValue(_state ? _state->gridSpacing() : 50);
    _grid_spacing_spinbox->setSuffix(" px");
    numeric_layout->addRow("Grid Spacing:", _grid_spacing_spinbox);

    main_layout->addWidget(numeric_group);

    // === Text Group ===
    auto * text_group = new QGroupBox("Label");
    auto * text_layout = new QFormLayout(text_group);

    _label_text_edit = new QLineEdit();
    _label_text_edit->setText(_state ? _state->labelText() : "Test Label");
    _label_text_edit->setPlaceholderText("Enter label text...");
    text_layout->addRow("Text:", _label_text_edit);

    main_layout->addWidget(text_group);

    // === Spacer ===
    main_layout->addStretch();

    // Set size policy
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setMinimumWidth(200);
}

void TestWidgetProperties::connectUiSignals() {
    connect(_show_grid_checkbox, &QCheckBox::toggled,
            this, &TestWidgetProperties::onShowGridToggled);
    connect(_show_crosshair_checkbox, &QCheckBox::toggled,
            this, &TestWidgetProperties::onShowCrosshairToggled);
    connect(_enable_animation_checkbox, &QCheckBox::toggled,
            this, &TestWidgetProperties::onEnableAnimationToggled);
    connect(_color_button, &QPushButton::clicked,
            this, &TestWidgetProperties::onColorButtonClicked);
    connect(_zoom_slider, &QSlider::valueChanged,
            this, &TestWidgetProperties::onZoomSliderChanged);
    connect(_grid_spacing_spinbox, qOverload<int>(&QSpinBox::valueChanged),
            this, &TestWidgetProperties::onGridSpacingChanged);
    connect(_label_text_edit, &QLineEdit::textChanged,
            this, &TestWidgetProperties::onLabelTextChanged);
}

void TestWidgetProperties::connectStateSignals() {
    if (!_state) {
        return;
    }

    connect(_state.get(), &TestWidgetState::showGridChanged,
            this, &TestWidgetProperties::onStateShowGridChanged);
    connect(_state.get(), &TestWidgetState::showCrosshairChanged,
            this, &TestWidgetProperties::onStateShowCrosshairChanged);
    connect(_state.get(), &TestWidgetState::enableAnimationChanged,
            this, &TestWidgetProperties::onStateEnableAnimationChanged);
    connect(_state.get(), &TestWidgetState::highlightColorChanged,
            this, &TestWidgetProperties::onStateHighlightColorChanged);
    connect(_state.get(), &TestWidgetState::zoomLevelChanged,
            this, &TestWidgetProperties::onStateZoomLevelChanged);
    connect(_state.get(), &TestWidgetState::gridSpacingChanged,
            this, &TestWidgetProperties::onStateGridSpacingChanged);
    connect(_state.get(), &TestWidgetState::labelTextChanged,
            this, &TestWidgetProperties::onStateLabelTextChanged);
}

void TestWidgetProperties::updateColorButtonStyle() {
    if (!_color_button || !_state) {
        return;
    }

    QColor const color = _state->highlightColor();
    QString const style = QString(
        "QPushButton { background-color: %1; border: 1px solid #555; border-radius: 3px; }"
    ).arg(color.name());
    _color_button->setStyleSheet(style);
}

// === UI Event Handlers ===

void TestWidgetProperties::onShowGridToggled(bool checked) {
    if (_updating_from_state || !_state) {
        return;
    }
    _state->setShowGrid(checked);
}

void TestWidgetProperties::onShowCrosshairToggled(bool checked) {
    if (_updating_from_state || !_state) {
        return;
    }
    _state->setShowCrosshair(checked);
}

void TestWidgetProperties::onEnableAnimationToggled(bool checked) {
    if (_updating_from_state || !_state) {
        return;
    }
    _state->setEnableAnimation(checked);
}

void TestWidgetProperties::onColorButtonClicked() {
    if (!_state) {
        return;
    }

    QColor const current = _state->highlightColor();
    QColor const selected = QColorDialog::getColor(current, this, "Select Highlight Color");

    if (selected.isValid()) {
        _state->setHighlightColor(selected);
    }
}

void TestWidgetProperties::onZoomSliderChanged(int value) {
    if (_updating_from_state || !_state) {
        return;
    }

    double const zoom = value / 100.0;
    _state->setZoomLevel(zoom);
    _zoom_label->setText(QString::number(zoom, 'f', 1) + "x");
}

void TestWidgetProperties::onGridSpacingChanged(int value) {
    if (_updating_from_state || !_state) {
        return;
    }
    _state->setGridSpacing(value);
}

void TestWidgetProperties::onLabelTextChanged(QString const & text) {
    if (_updating_from_state || !_state) {
        return;
    }
    _state->setLabelText(text);
}

// === State Change Handlers ===

void TestWidgetProperties::onStateShowGridChanged(bool show) {
    _updating_from_state = true;
    _show_grid_checkbox->setChecked(show);
    _updating_from_state = false;
}

void TestWidgetProperties::onStateShowCrosshairChanged(bool show) {
    _updating_from_state = true;
    _show_crosshair_checkbox->setChecked(show);
    _updating_from_state = false;
}

void TestWidgetProperties::onStateEnableAnimationChanged(bool enable) {
    _updating_from_state = true;
    _enable_animation_checkbox->setChecked(enable);
    _updating_from_state = false;
}

void TestWidgetProperties::onStateHighlightColorChanged(QColor const & /*color*/) {
    _updating_from_state = true;
    updateColorButtonStyle();
    _updating_from_state = false;
}

void TestWidgetProperties::onStateZoomLevelChanged(double zoom) {
    _updating_from_state = true;
    _zoom_slider->setValue(static_cast<int>(zoom * 100));
    _zoom_label->setText(QString::number(zoom, 'f', 1) + "x");
    _updating_from_state = false;
}

void TestWidgetProperties::onStateGridSpacingChanged(int spacing) {
    _updating_from_state = true;
    _grid_spacing_spinbox->setValue(spacing);
    _updating_from_state = false;
}

void TestWidgetProperties::onStateLabelTextChanged(QString const & text) {
    _updating_from_state = true;
    _label_text_edit->setText(text);
    _updating_from_state = false;
}
