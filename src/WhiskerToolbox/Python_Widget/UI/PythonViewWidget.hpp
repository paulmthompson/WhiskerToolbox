#ifndef PYTHON_VIEW_WIDGET_HPP
#define PYTHON_VIEW_WIDGET_HPP

/**
 * @file PythonViewWidget.hpp
 * @brief Main view widget for the Python integration
 *
 * PythonViewWidget is a tabbed container holding:
 * - Console tab (PythonConsoleWidget) — interactive REPL
 * - Editor tab (PythonEditorWidget) — script file editor
 *
 * It owns the PythonEngine and PythonBridge (singleton pattern, since
 * allow_multiple = false for PythonWidget).
 *
 * @see PythonConsoleWidget for the REPL console
 * @see PythonEditorWidget for the script editor
 * @see PythonBridge for the DataManager ↔ Python bridge
 */

#include <QWidget>

#include <memory>

class DataManager;
class PythonBridge;
class PythonConsoleWidget;
class PythonEditorWidget;
class PythonEngine;
class PythonWidgetState;
class QTabWidget;

class PythonViewWidget : public QWidget {
    Q_OBJECT

public:
    explicit PythonViewWidget(std::shared_ptr<PythonWidgetState> state,
                              std::shared_ptr<DataManager> data_manager,
                              QWidget * parent = nullptr);
    ~PythonViewWidget() override;

    [[nodiscard]] std::shared_ptr<PythonWidgetState> getState() const { return _state; }

    /// Access to the bridge (needed by properties widget)
    [[nodiscard]] PythonBridge * bridge() const { return _bridge.get(); }

    /// Access the console widget (for inserting text from properties)
    [[nodiscard]] PythonConsoleWidget * consoleWidget() const { return _console; }

    /// Access the editor widget
    [[nodiscard]] PythonEditorWidget * editorWidget() const { return _editor; }

signals:
    /// Forwarded from console/editor after execution
    void executionFinished();

private:
    void _setupUI();
    void _connectSignals();

    std::shared_ptr<PythonWidgetState> _state;
    std::shared_ptr<DataManager> _data_manager;

    // Owned: engine → bridge (order matters for destruction)
    std::unique_ptr<PythonEngine> _engine;
    std::unique_ptr<PythonBridge> _bridge;

    // UI
    QTabWidget * _tab_widget{nullptr};
    PythonConsoleWidget * _console{nullptr};
    PythonEditorWidget * _editor{nullptr};
};

#endif // PYTHON_VIEW_WIDGET_HPP
