#ifndef PYTHON_WIDGET_STATE_HPP
#define PYTHON_WIDGET_STATE_HPP

/**
 * @file PythonWidgetState.hpp
 * @brief State class for Python_Widget
 *
 * PythonWidgetState manages the serializable state for the Python integration widget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 *
 * @see EditorState for base class documentation
 * @see PythonWidgetStateData for the complete state structure
 */

#include "EditorState/EditorState.hpp"
#include "PythonWidgetStateData.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>

class PythonWidgetState : public EditorState {
    Q_OBJECT

public:
    explicit PythonWidgetState(QObject * parent = nullptr);
    ~PythonWidgetState() override = default;

    // === Type Identification ===

    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("PythonWidget"); }
    [[nodiscard]] QString getDisplayName() const override;
    void setDisplayName(QString const & name) override;

    // === Serialization ===

    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

    // === Direct Data Access ===

    [[nodiscard]] PythonWidgetStateData const & data() const { return _data; }

    // === State Accessors ===

    [[nodiscard]] QString lastScriptPath() const { return QString::fromStdString(_data.last_script_path); }
    void setLastScriptPath(QString const & path);

    [[nodiscard]] bool autoScroll() const { return _data.auto_scroll; }
    void setAutoScroll(bool enabled);

    [[nodiscard]] int fontSize() const { return _data.font_size; }
    void setFontSize(int size);

signals:
    void lastScriptPathChanged(QString const & path);
    void autoScrollChanged(bool enabled);
    void fontSizeChanged(int size);

private:
    PythonWidgetStateData _data;
};

#endif // PYTHON_WIDGET_STATE_HPP
