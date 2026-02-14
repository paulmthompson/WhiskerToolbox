// Python/pybind11 headers MUST come before Qt headers to avoid 'slots' macro conflict
#include "PythonBridge.hpp"
#include "PythonEngine.hpp"

#include "PythonViewWidget.hpp"
#include "PythonConsoleWidget.hpp"
#include "PythonEditorWidget.hpp"
#include "Python_Widget/Core/PythonWidgetState.hpp"

#include <QTabWidget>
#include <QVBoxLayout>

#include <iostream>

PythonViewWidget::PythonViewWidget(std::shared_ptr<PythonWidgetState> state,
                                   std::shared_ptr<DataManager> data_manager,
                                   QWidget * parent)
    : QWidget(parent)
    , _state(std::move(state))
    , _data_manager(std::move(data_manager)) {

    // Create engine and bridge
    try {
        _engine = std::make_unique<PythonEngine>();
        _bridge = std::make_unique<PythonBridge>(_data_manager, *_engine);
        _bridge->exposeDataManager();

        // Phase 5: Execute auto-import prelude if enabled
        if (_state && _state->preludeEnabled()) {
            auto const prelude = _state->autoImportPrelude();
            if (!prelude.isEmpty()) {
                _engine->executePrelude(prelude.toStdString());
            }
        }

        // Phase 5: Restore working directory from state
        if (_state && !_state->lastWorkingDirectory().isEmpty()) {
            _engine->setWorkingDirectory(_state->lastWorkingDirectory().toStdString());
        }

        // Phase 5: Restore sys.argv from state
        if (_state && !_state->scriptArguments().isEmpty()) {
            _engine->setSysArgv(_state->scriptArguments().toStdString());
        }

        // Phase 6: Restore virtual environment from state
        if (_state && !_state->venvPath().isEmpty()) {
            auto const venv_path = _state->venvPath().toStdString();
            auto const error = _engine->activateVenv(venv_path);
            if (!error.empty()) {
                std::cerr << "PythonViewWidget: Failed to restore venv: "
                          << error << std::endl;
            }
        }
    } catch (std::exception const & e) {
        std::cerr << "PythonViewWidget: Failed to initialize Python engine: "
                  << e.what() << std::endl;
    }

    _setupUI();
    _connectSignals();
}

PythonViewWidget::~PythonViewWidget() {
    // Bridge must be destroyed before engine (it holds a reference)
    _bridge.reset();
    _engine.reset();
}

void PythonViewWidget::_setupUI() {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    _tab_widget = new QTabWidget(this);

    // Console tab
    _console = new PythonConsoleWidget(_state, _bridge.get(), this);
    _tab_widget->addTab(_console, QStringLiteral("Console"));

    // Editor tab
    _editor = new PythonEditorWidget(_state, _bridge.get(), this);
    _tab_widget->addTab(_editor, QStringLiteral("Editor"));

    layout->addWidget(_tab_widget);
}

void PythonViewWidget::_connectSignals() {
    // Forward execution signals
    connect(_console, &PythonConsoleWidget::executionFinished,
            this, &PythonViewWidget::executionFinished);
    connect(_editor, &PythonEditorWidget::executionFinished,
            this, &PythonViewWidget::executionFinished);

    // Editor output → console output area
    connect(_editor, &PythonEditorWidget::outputGenerated,
            this, [this](QString const & stdout_text,
                         QString const & stderr_text,
                         bool /*success*/) {
                // Show script output in the console tab
                if (!stdout_text.isEmpty() || !stderr_text.isEmpty()) {
                    // Switch to console to see output
                    _tab_widget->setCurrentWidget(_console);
                }
            });

    // State connections
    if (_state) {
        connect(_state.get(), &PythonWidgetState::fontSizeChanged,
                _console, &PythonConsoleWidget::updateFontSize);
        connect(_state.get(), &PythonWidgetState::fontSizeChanged,
                _editor, &PythonEditorWidget::updateFontSize);
        connect(_state.get(), &PythonWidgetState::autoScrollChanged,
                _console, &PythonConsoleWidget::setAutoScroll);
        connect(_state.get(), &PythonWidgetState::showLineNumbersChanged,
                _editor, &PythonEditorWidget::setShowLineNumbers);
    }
}
