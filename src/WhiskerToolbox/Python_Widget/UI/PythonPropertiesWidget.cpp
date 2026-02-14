// Python/pybind11 headers MUST come before Qt headers to avoid 'slots' macro conflict
#include "PythonBridge.hpp"
#include "PythonEngine.hpp"

#include "PythonPropertiesWidget.hpp"
#include "Python_Widget/Core/PythonWidgetState.hpp"

#include "DataManager/DataManager.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

PythonPropertiesWidget::PythonPropertiesWidget(std::shared_ptr<PythonWidgetState> state,
                                               PythonBridge * bridge,
                                               std::shared_ptr<DataManager> data_manager,
                                               QWidget * parent)
    : QWidget(parent)
    , _state(std::move(state))
    , _bridge(bridge)
    , _data_manager(std::move(data_manager)) {
    _setupUI();
}

PythonPropertiesWidget::~PythonPropertiesWidget() = default;

void PythonPropertiesWidget::_setupUI() {
    // Scrollable container
    auto * scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto * container = new QWidget();
    auto * layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // === Environment Section ===
    auto * env_group = new QGroupBox(QStringLiteral("Environment"), container);
    auto * env_layout = new QVBoxLayout(env_group);

    _python_version_label = new QLabel(env_group);
    if (_bridge) {
        auto const & engine = _bridge->engine();
        _python_version_label->setText(
            QStringLiteral("Python %1").arg(QString::fromStdString(engine.pythonVersion())));
    } else {
        _python_version_label->setText(QStringLiteral("Python (not initialized)"));
    }
    env_layout->addWidget(_python_version_label);

    // Working directory
    auto * cwd_label = new QLabel(QStringLiteral("Working Directory:"), env_group);
    env_layout->addWidget(cwd_label);

    auto * cwd_row = new QHBoxLayout();
    _working_dir_edit = new QLineEdit(env_group);
    _working_dir_edit->setPlaceholderText(QStringLiteral("(default: script parent directory)"));
    if (_state && !_state->lastWorkingDirectory().isEmpty()) {
        _working_dir_edit->setText(_state->lastWorkingDirectory());
    } else if (_bridge) {
        auto const cwd = _bridge->engine().getWorkingDirectory();
        _working_dir_edit->setText(QString::fromStdString(cwd.string()));
    }
    cwd_row->addWidget(_working_dir_edit);

    auto * browse_button = new QPushButton(QStringLiteral("..."), env_group);
    browse_button->setFixedWidth(30);
    browse_button->setToolTip(QStringLiteral("Browse for working directory"));
    connect(browse_button, &QPushButton::clicked,
            this, &PythonPropertiesWidget::_onBrowseWorkingDirectory);
    cwd_row->addWidget(browse_button);

    auto * apply_cwd_button = new QPushButton(QStringLiteral("Set"), env_group);
    apply_cwd_button->setFixedWidth(40);
    apply_cwd_button->setToolTip(QStringLiteral("Apply working directory"));
    connect(apply_cwd_button, &QPushButton::clicked,
            this, &PythonPropertiesWidget::_onApplyWorkingDirectory);
    cwd_row->addWidget(apply_cwd_button);

    env_layout->addLayout(cwd_row);
    layout->addWidget(env_group);

    // === Virtual Environment Section (Phase 6) ===
    auto * venv_group = new QGroupBox(QStringLiteral("Virtual Environment"), container);
    auto * venv_layout = new QVBoxLayout(venv_group);

    // Status label
    _venv_status_label = new QLabel(QStringLiteral("No venv active"), venv_group);
    _venv_status_label->setStyleSheet(QStringLiteral("font-weight: bold;"));
    venv_layout->addWidget(_venv_status_label);

    // Venv selector
    auto * venv_select_label = new QLabel(QStringLiteral("Select environment:"), venv_group);
    venv_layout->addWidget(venv_select_label);

    auto * venv_select_row = new QHBoxLayout();
    _venv_combo = new QComboBox(venv_group);
    _venv_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _venv_combo->setToolTip(QStringLiteral("Select a virtual environment to activate"));
    connect(_venv_combo, QOverload<int>::of(&QComboBox::activated),
            this, &PythonPropertiesWidget::_onVenvSelected);
    venv_select_row->addWidget(_venv_combo);

    auto * browse_venv_button = new QPushButton(QStringLiteral("..."), venv_group);
    browse_venv_button->setFixedWidth(30);
    browse_venv_button->setToolTip(QStringLiteral("Browse for virtual environment directory"));
    connect(browse_venv_button, &QPushButton::clicked,
            this, &PythonPropertiesWidget::_onBrowseVenv);
    venv_select_row->addWidget(browse_venv_button);

    auto * refresh_venv_button = new QPushButton(QStringLiteral("↻"), venv_group);
    refresh_venv_button->setFixedWidth(30);
    refresh_venv_button->setToolTip(QStringLiteral("Refresh virtual environment list"));
    connect(refresh_venv_button, &QPushButton::clicked,
            this, &PythonPropertiesWidget::_onRefreshVenvs);
    venv_select_row->addWidget(refresh_venv_button);
    venv_layout->addLayout(venv_select_row);

    auto * deactivate_button = new QPushButton(QStringLiteral("Deactivate"), venv_group);
    deactivate_button->setToolTip(QStringLiteral("Deactivate the current virtual environment"));
    connect(deactivate_button, &QPushButton::clicked,
            this, &PythonPropertiesWidget::_onDeactivateVenv);
    venv_layout->addWidget(deactivate_button);

    // Package list
    auto * pkg_label = new QLabel(QStringLiteral("Installed Packages:"), venv_group);
    venv_layout->addWidget(pkg_label);

    _packages_tree = new QTreeWidget(venv_group);
    _packages_tree->setHeaderLabels({QStringLiteral("Package"), QStringLiteral("Version")});
    _packages_tree->setRootIsDecorated(false);
    _packages_tree->setAlternatingRowColors(true);
    _packages_tree->header()->setStretchLastSection(true);
    _packages_tree->setMaximumHeight(150);
    venv_layout->addWidget(_packages_tree);

    auto * refresh_pkg_button = new QPushButton(QStringLiteral("Refresh Packages"), venv_group);
    connect(refresh_pkg_button, &QPushButton::clicked,
            this, &PythonPropertiesWidget::_onRefreshPackages);
    venv_layout->addWidget(refresh_pkg_button);

    // Install package
    auto * install_row = new QHBoxLayout();
    _install_package_edit = new QLineEdit(venv_group);
    _install_package_edit->setPlaceholderText(QStringLiteral("e.g. numpy, scipy>=1.10"));
    install_row->addWidget(_install_package_edit);

    auto * install_button = new QPushButton(QStringLiteral("Install"), venv_group);
    install_button->setToolTip(QStringLiteral("Install package using pip"));
    connect(install_button, &QPushButton::clicked,
            this, &PythonPropertiesWidget::_onInstallPackage);
    install_row->addWidget(install_button);
    venv_layout->addLayout(install_row);

    layout->addWidget(venv_group);

    // Populate venv dropdown and update indicator
    _populateVenvCombo();
    _updateVenvIndicator();

    // === Settings Section ===
    auto * settings_group = new QGroupBox(QStringLiteral("Settings"), container);
    auto * settings_layout = new QVBoxLayout(settings_group);

    // Font size
    auto * font_row = new QHBoxLayout();
    font_row->addWidget(new QLabel(QStringLiteral("Font Size:"), settings_group));
    _font_size_spin = new QSpinBox(settings_group);
    _font_size_spin->setRange(8, 24);
    _font_size_spin->setValue(_state ? _state->fontSize() : 10);
    connect(_font_size_spin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PythonPropertiesWidget::_onFontSizeChanged);
    font_row->addWidget(_font_size_spin);
    font_row->addStretch();
    settings_layout->addLayout(font_row);

    // Auto-scroll
    _auto_scroll_check = new QCheckBox(QStringLiteral("Auto-scroll output"), settings_group);
    _auto_scroll_check->setChecked(_state ? _state->autoScroll() : true);
    connect(_auto_scroll_check, &QCheckBox::toggled,
            this, &PythonPropertiesWidget::_onAutoScrollChanged);
    settings_layout->addWidget(_auto_scroll_check);

    // Line numbers
    _line_numbers_check = new QCheckBox(QStringLiteral("Show line numbers"), settings_group);
    _line_numbers_check->setChecked(_state ? _state->showLineNumbers() : true);
    connect(_line_numbers_check, &QCheckBox::toggled,
            this, &PythonPropertiesWidget::_onShowLineNumbersChanged);
    settings_layout->addWidget(_line_numbers_check);

    layout->addWidget(settings_group);

    // === Script Arguments Section (Phase 5) ===
    auto * args_group = new QGroupBox(QStringLiteral("Script Arguments"), container);
    auto * args_layout = new QVBoxLayout(args_group);

    auto * args_label = new QLabel(
        QStringLiteral("Arguments passed as sys.argv (space-separated):"), args_group);
    args_layout->addWidget(args_label);

    _script_args_edit = new QLineEdit(args_group);
    _script_args_edit->setPlaceholderText(QStringLiteral("e.g. --input data.csv --output results.csv"));
    if (_state) {
        _script_args_edit->setText(_state->scriptArguments());
    }
    connect(_script_args_edit, &QLineEdit::editingFinished,
            this, &PythonPropertiesWidget::_onScriptArgumentsChanged);
    args_layout->addWidget(_script_args_edit);

    layout->addWidget(args_group);

    // === Auto-Import Prelude Section (Phase 5) ===
    auto * prelude_group = new QGroupBox(QStringLiteral("Auto-Import Prelude"), container);
    auto * prelude_layout = new QVBoxLayout(prelude_group);

    _prelude_enabled_check = new QCheckBox(
        QStringLiteral("Execute prelude on interpreter start/reset"), prelude_group);
    _prelude_enabled_check->setChecked(_state ? _state->preludeEnabled() : true);
    connect(_prelude_enabled_check, &QCheckBox::toggled,
            this, &PythonPropertiesWidget::_onPreludeEnabledChanged);
    prelude_layout->addWidget(_prelude_enabled_check);

    _prelude_edit = new QPlainTextEdit(prelude_group);
    _prelude_edit->setMaximumHeight(100);
    QFont mono_font(QStringLiteral("Courier New"));
    mono_font.setStyleHint(QFont::Monospace);
    mono_font.setPointSize(9);
    _prelude_edit->setFont(mono_font);
    if (_state) {
        _prelude_edit->setPlainText(_state->autoImportPrelude());
    }
    prelude_layout->addWidget(_prelude_edit);

    auto * apply_prelude_button = new QPushButton(QStringLiteral("Apply && Run Prelude"), prelude_group);
    apply_prelude_button->setToolTip(QStringLiteral("Save prelude and execute it now"));
    connect(apply_prelude_button, &QPushButton::clicked,
            this, &PythonPropertiesWidget::_onApplyPrelude);
    prelude_layout->addWidget(apply_prelude_button);

    layout->addWidget(prelude_group);

    // === Namespace Section ===
    auto * ns_group = new QGroupBox(QStringLiteral("Namespace"), container);
    auto * ns_layout = new QVBoxLayout(ns_group);

    _namespace_tree = new QTreeWidget(ns_group);
    _namespace_tree->setHeaderLabels({QStringLiteral("Name"), QStringLiteral("Type")});
    _namespace_tree->setRootIsDecorated(false);
    _namespace_tree->setAlternatingRowColors(true);
    _namespace_tree->header()->setStretchLastSection(true);
    _namespace_tree->setMaximumHeight(200);
    ns_layout->addWidget(_namespace_tree);

    _clear_namespace_button = new QPushButton(QStringLiteral("Clear Namespace"), ns_group);
    _clear_namespace_button->setToolTip(QStringLiteral("Reset all user-defined variables"));
    connect(_clear_namespace_button, &QPushButton::clicked,
            this, &PythonPropertiesWidget::_onClearNamespace);
    ns_layout->addWidget(_clear_namespace_button);

    layout->addWidget(ns_group);

    // === Data Keys Section ===
    auto * data_group = new QGroupBox(QStringLiteral("Data Keys"), container);
    auto * data_layout = new QVBoxLayout(data_group);

    _data_keys_tree = new QTreeWidget(data_group);
    _data_keys_tree->setHeaderLabels({QStringLiteral("Key"), QStringLiteral("Type")});
    _data_keys_tree->setRootIsDecorated(false);
    _data_keys_tree->setAlternatingRowColors(true);
    _data_keys_tree->header()->setStretchLastSection(true);
    _data_keys_tree->setMaximumHeight(200);
    data_layout->addWidget(_data_keys_tree);

    auto * insert_button = new QPushButton(QStringLiteral("Insert dm.getData(...)"), data_group);
    insert_button->setToolTip(QStringLiteral("Insert getData code for selected key into console"));
    connect(insert_button, &QPushButton::clicked, this, [this]() {
        auto * item = _data_keys_tree->currentItem();
        if (item) {
            QString const key = item->text(0);
            emit insertCodeRequested(
                QStringLiteral("dm.getData(\"%1\")").arg(key));
        }
    });
    data_layout->addWidget(insert_button);

    layout->addWidget(data_group);

    // Spacer at bottom
    layout->addStretch();

    scroll->setWidget(container);

    auto * outer_layout = new QVBoxLayout(this);
    outer_layout->setContentsMargins(0, 0, 0, 0);
    outer_layout->addWidget(scroll);

    // Do initial refresh
    refreshNamespace();
}

void PythonPropertiesWidget::refreshNamespace() {
    _namespace_tree->clear();

    if (_bridge) {
        auto const & engine = _bridge->engine();
        auto var_names = engine.getUserVariableNames();

        for (auto const & name : var_names) {
            // Get type name via Python print()
            auto result = _bridge->execute(
                "print(type(" + name + ").__name__, end='')");
            QString type_str = QString::fromStdString(result.stdout_text).trimmed();

            auto * item = new QTreeWidgetItem(
                {QString::fromStdString(name), type_str});
            _namespace_tree->addTopLevelItem(item);
        }

        // Update working directory display
        if (_working_dir_edit) {
            auto const cwd = engine.getWorkingDirectory();
            _working_dir_edit->setText(QString::fromStdString(cwd.string()));
        }
    }

    _refreshDataKeys();
}

void PythonPropertiesWidget::_refreshDataKeys() {
    _data_keys_tree->clear();

    if (!_data_manager) {
        return;
    }

    auto keys = _data_manager->getAllKeys();
    for (auto const & key : keys) {
        QString type_str = QString::fromStdString(
            convert_data_type_to_string(_data_manager->getType(key)));

        auto * item = new QTreeWidgetItem(
            {QString::fromStdString(key), type_str});
        _data_keys_tree->addTopLevelItem(item);
    }
}

void PythonPropertiesWidget::_onFontSizeChanged(int size) {
    if (_state) {
        _state->setFontSize(size);
    }
}

void PythonPropertiesWidget::_onAutoScrollChanged(bool enabled) {
    if (_state) {
        _state->setAutoScroll(enabled);
    }
}

void PythonPropertiesWidget::_onShowLineNumbersChanged(bool enabled) {
    if (_state) {
        _state->setShowLineNumbers(enabled);
    }
}

void PythonPropertiesWidget::_onClearNamespace() {
    if (_bridge) {
        _bridge->engine().resetNamespace();
        _bridge->exposeDataManager();  // Re-inject dm after namespace clear

        // Re-run prelude if enabled
        if (_state && _state->preludeEnabled()) {
            auto const prelude = _state->autoImportPrelude();
            if (!prelude.isEmpty()) {
                _bridge->engine().executePrelude(prelude.toStdString());
            }
        }

        refreshNamespace();
    }
}

// === Phase 5: Working Directory ===

void PythonPropertiesWidget::_onBrowseWorkingDirectory() {
    QString const current = _working_dir_edit->text();
    QString const dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Select Working Directory"),
        current.isEmpty() ? QDir::homePath() : current);

    if (!dir.isEmpty()) {
        _working_dir_edit->setText(dir);
        _onApplyWorkingDirectory();
    }
}

void PythonPropertiesWidget::_onApplyWorkingDirectory() {
    QString const dir = _working_dir_edit->text().trimmed();
    if (dir.isEmpty()) {
        return;
    }

    if (_bridge) {
        _bridge->engine().setWorkingDirectory(dir.toStdString());
    }
    if (_state) {
        _state->setLastWorkingDirectory(dir);
    }
}

// === Phase 5: Script Arguments ===

void PythonPropertiesWidget::_onScriptArgumentsChanged() {
    QString const args = _script_args_edit->text().trimmed();
    if (_state) {
        _state->setScriptArguments(args);
    }
    if (_bridge) {
        _bridge->engine().setSysArgv(args.toStdString());
    }
}

// === Phase 5: Auto-Import Prelude ===

void PythonPropertiesWidget::_onPreludeEnabledChanged(bool enabled) {
    if (_state) {
        _state->setPreludeEnabled(enabled);
    }
    if (_prelude_edit) {
        _prelude_edit->setEnabled(enabled);
    }
}

void PythonPropertiesWidget::_onApplyPrelude() {
    QString const prelude = _prelude_edit->toPlainText();
    if (_state) {
        _state->setAutoImportPrelude(prelude);
    }

    if (_bridge && !prelude.isEmpty()) {
        auto result = _bridge->engine().executePrelude(prelude.toStdString());
        if (!result.success) {
            // Show error in a tooltip or status message
            _prelude_edit->setToolTip(
                QStringLiteral("Prelude error:\n%1")
                    .arg(QString::fromStdString(result.stderr_text)));
        } else {
            _prelude_edit->setToolTip(QStringLiteral("Prelude executed successfully"));
        }
    }
}

// === Phase 6: Virtual Environment ===

void PythonPropertiesWidget::_populateVenvCombo() {
    if (!_venv_combo) {
        return;
    }
    _venv_combo->clear();
    _venv_combo->addItem(QStringLiteral("(none)"), QString{});

    if (!_bridge) {
        return;
    }

    auto const venvs = _bridge->engine().discoverVenvs();
    for (auto const & venv : venvs) {
        auto const name = QString::fromStdString(venv.filename().string());
        auto const path = QString::fromStdString(venv.string());
        _venv_combo->addItem(name, path);
        _venv_combo->setItemData(_venv_combo->count() - 1, path, Qt::ToolTipRole);
    }

    // Select the currently active venv (from state)
    if (_state && !_state->venvPath().isEmpty()) {
        int const idx = _venv_combo->findData(_state->venvPath());
        if (idx >= 0) {
            _venv_combo->setCurrentIndex(idx);
        } else {
            // Add it manually if not in discovered list
            auto const venv_path = _state->venvPath();
            auto const name = QFileInfo(venv_path).fileName();
            _venv_combo->addItem(name, venv_path);
            _venv_combo->setCurrentIndex(_venv_combo->count() - 1);
        }
    }
}

void PythonPropertiesWidget::_updateVenvIndicator() {
    if (!_venv_status_label || !_bridge) {
        return;
    }

    if (_bridge->engine().isVenvActive()) {
        auto const path = _bridge->engine().activeVenvPath();
        auto const name = QString::fromStdString(path.filename().string());
        _venv_status_label->setText(QStringLiteral("Active: %1").arg(name));
        _venv_status_label->setStyleSheet(QStringLiteral("font-weight: bold; color: green;"));
    } else {
        _venv_status_label->setText(QStringLiteral("No venv active"));
        _venv_status_label->setStyleSheet(QStringLiteral("font-weight: bold; color: gray;"));
    }
}

void PythonPropertiesWidget::_onVenvSelected(int index) {
    if (!_venv_combo || !_bridge) {
        return;
    }

    auto const venv_path = _venv_combo->itemData(index).toString();
    if (venv_path.isEmpty()) {
        // "(none)" selected — deactivate
        _onDeactivateVenv();
        return;
    }

    // Validate first
    auto error = _bridge->engine().validateVenv(venv_path.toStdString());
    if (!error.empty()) {
        QMessageBox::warning(this,
                             QStringLiteral("Virtual Environment Error"),
                             QString::fromStdString(error));
        return;
    }

    // Activate
    error = _bridge->engine().activateVenv(venv_path.toStdString());
    if (!error.empty()) {
        QMessageBox::warning(this,
                             QStringLiteral("Activation Failed"),
                             QString::fromStdString(error));
        return;
    }

    // Save to state
    if (_state) {
        _state->setVenvPath(venv_path);
    }

    _updateVenvIndicator();
    _onRefreshPackages();
}

void PythonPropertiesWidget::_onBrowseVenv() {
    QString const dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Select Virtual Environment Directory"),
        QDir::homePath());

    if (dir.isEmpty()) {
        return;
    }

    if (!_bridge) {
        return;
    }

    // Validate
    auto const error = _bridge->engine().validateVenv(dir.toStdString());
    if (!error.empty()) {
        QMessageBox::warning(this,
                             QStringLiteral("Invalid Virtual Environment"),
                             QString::fromStdString(error));
        return;
    }

    // Add to combo if not already present
    int idx = _venv_combo->findData(dir);
    if (idx < 0) {
        auto const name = QFileInfo(dir).fileName();
        _venv_combo->addItem(name, dir);
        idx = _venv_combo->count() - 1;
    }
    _venv_combo->setCurrentIndex(idx);
    _onVenvSelected(idx);
}

void PythonPropertiesWidget::_onRefreshVenvs() {
    _populateVenvCombo();
}

void PythonPropertiesWidget::_onDeactivateVenv() {
    if (!_bridge) {
        return;
    }

    _bridge->engine().deactivateVenv();

    if (_state) {
        _state->setVenvPath(QString{});
    }

    if (_venv_combo) {
        _venv_combo->setCurrentIndex(0);  // "(none)"
    }

    _updateVenvIndicator();

    // Clear package list
    if (_packages_tree) {
        _packages_tree->clear();
    }
}

void PythonPropertiesWidget::_onRefreshPackages() {
    if (!_packages_tree || !_bridge) {
        return;
    }

    _packages_tree->clear();

    auto const packages = _bridge->engine().listInstalledPackages();
    for (auto const & [name, version] : packages) {
        auto * item = new QTreeWidgetItem({
            QString::fromStdString(name),
            QString::fromStdString(version)});
        _packages_tree->addTopLevelItem(item);
    }
}

void PythonPropertiesWidget::_onInstallPackage() {
    if (!_install_package_edit || !_bridge) {
        return;
    }

    QString const package = _install_package_edit->text().trimmed();
    if (package.isEmpty()) {
        return;
    }

    // Confirm with user
    auto const reply = QMessageBox::question(
        this,
        QStringLiteral("Install Package"),
        QStringLiteral("Install \"%1\" using pip?\n\nThis may take a moment.").arg(package),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    setCursor(Qt::WaitCursor);
    auto result = _bridge->engine().installPackage(package.toStdString());
    unsetCursor();

    if (result.success) {
        QMessageBox::information(this,
                                 QStringLiteral("Package Installed"),
                                 QStringLiteral("Successfully installed \"%1\".").arg(package));
        _install_package_edit->clear();
        _onRefreshPackages();
    } else {
        QMessageBox::warning(this,
                             QStringLiteral("Installation Failed"),
                             QStringLiteral("Failed to install \"%1\":\n\n%2")
                                 .arg(package, QString::fromStdString(result.stderr_text)));
    }
}
