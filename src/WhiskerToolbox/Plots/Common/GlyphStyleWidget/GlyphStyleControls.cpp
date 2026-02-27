#include "GlyphStyleControls.hpp"
#include "Core/GlyphStyleState.hpp"

#include "CorePlotting/DataTypes/GlyphStyleData.hpp"

#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPushButton>

#include <cmath>

GlyphStyleControls::GlyphStyleControls(
    GlyphStyleState * state,
    QWidget * parent)
    : QWidget(parent),
      _state(state),
      _type_combo(nullptr),
      _size_spinbox(nullptr),
      _color_button(nullptr),
      _alpha_spinbox(nullptr),
      _updating_ui(false)
{
    auto * layout = new QFormLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Glyph type combo box
    _type_combo = new QComboBox(this);
    _type_combo->addItem("Circle");
    _type_combo->addItem("Square");
    _type_combo->addItem("Tick");
    _type_combo->addItem("Cross");
    _type_combo->addItem("Diamond");
    _type_combo->addItem("Triangle");
    layout->addRow("Shape:", _type_combo);

    // Size spin box
    _size_spinbox = new QDoubleSpinBox(this);
    _size_spinbox->setMinimum(0.5);
    _size_spinbox->setMaximum(50.0);
    _size_spinbox->setSingleStep(0.5);
    _size_spinbox->setDecimals(1);
    _size_spinbox->setSuffix(" px");
    layout->addRow("Size:", _size_spinbox);

    // Color button
    _color_button = new QPushButton(this);
    _color_button->setFixedSize(60, 24);
    _color_button->setCursor(Qt::PointingHandCursor);
    layout->addRow("Color:", _color_button);

    // Alpha spin box
    _alpha_spinbox = new QDoubleSpinBox(this);
    _alpha_spinbox->setMinimum(0.0);
    _alpha_spinbox->setMaximum(1.0);
    _alpha_spinbox->setSingleStep(0.05);
    _alpha_spinbox->setDecimals(2);
    layout->addRow("Alpha:", _alpha_spinbox);

    // Connect UI signals
    connect(_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GlyphStyleControls::onGlyphTypeChanged);
    connect(_size_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GlyphStyleControls::onSizeChanged);
    connect(_color_button, &QPushButton::clicked,
            this, &GlyphStyleControls::onColorClicked);
    connect(_alpha_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GlyphStyleControls::onAlphaChanged);

    // Connect to state signals
    if (_state) {
        connect(_state, &GlyphStyleState::styleChanged,
                this, &GlyphStyleControls::onStateStyleChanged);
        connect(_state, &GlyphStyleState::styleUpdated,
                this, &GlyphStyleControls::onStateStyleUpdated);
    }

    // Initialize UI from state
    updateFromState();
}

void GlyphStyleControls::onGlyphTypeChanged(int index)
{
    if (_updating_ui || !_state) return;

    // Combo box indices match CorePlotting::GlyphType enum values
    _state->setGlyphType(static_cast<CorePlotting::GlyphType>(index));
}

void GlyphStyleControls::onSizeChanged(double value)
{
    if (_updating_ui || !_state) return;

    _state->setSize(static_cast<float>(value));
}

void GlyphStyleControls::onColorClicked()
{
    if (!_state) return;

    QColor const current = QColor(QString::fromStdString(_state->hexColor()));
    QColor const chosen = QColorDialog::getColor(current, this, "Select Glyph Color");

    if (chosen.isValid()) {
        _state->setHexColor(chosen.name().toStdString());
    }
}

void GlyphStyleControls::onAlphaChanged(double value)
{
    if (_updating_ui || !_state) return;

    _state->setAlpha(static_cast<float>(value));
}

void GlyphStyleControls::onStateStyleChanged()
{
    updateFromState();
}

void GlyphStyleControls::onStateStyleUpdated()
{
    updateFromState();
}

void GlyphStyleControls::updateFromState()
{
    if (!_state) {
        // Clear/reset UI to defaults when no state is bound
        _updating_ui = true;
        _type_combo->setCurrentIndex(0);
        _size_spinbox->setValue(8.0);
        _alpha_spinbox->setValue(1.0);
        _color_button->setStyleSheet("background-color: #007bff; border: 1px solid #888;");
        _updating_ui = false;
        return;
    }

    _updating_ui = true;

    auto const & data = _state->data();

    // Update combo box
    int const type_index = static_cast<int>(data.glyph_type);
    if (_type_combo->currentIndex() != type_index) {
        _type_combo->setCurrentIndex(type_index);
    }

    // Update size
    if (std::abs(_size_spinbox->value() - static_cast<double>(data.size)) > 0.01) {
        _size_spinbox->setValue(static_cast<double>(data.size));
    }

    // Update alpha
    if (std::abs(_alpha_spinbox->value() - static_cast<double>(data.alpha)) > 0.01) {
        _alpha_spinbox->setValue(static_cast<double>(data.alpha));
    }

    // Update color button
    updateColorButtonStyle();

    _updating_ui = false;
}

void GlyphStyleControls::updateColorButtonStyle()
{
    if (!_state) return;

    QString const hex = QString::fromStdString(_state->hexColor());
    _color_button->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid #888;").arg(hex));
    _color_button->setToolTip(hex);
}

void GlyphStyleControls::setGlyphStyleState(GlyphStyleState * state)
{
    if (_state) {
        _state->disconnect(this);
    }
    _state = state;
    if (_state) {
        connect(_state, &GlyphStyleState::styleChanged,
                this, &GlyphStyleControls::onStateStyleChanged);
        connect(_state, &GlyphStyleState::styleUpdated,
                this, &GlyphStyleControls::onStateStyleUpdated);
    }
    setEnabled(_state != nullptr);
    updateFromState();
}
