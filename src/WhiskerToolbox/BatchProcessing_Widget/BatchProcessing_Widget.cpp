#include "BatchProcessing_Widget.hpp"
#include "BatchProcessingState.hpp"
#include "ui_BatchProcessing_Widget.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextEdit>
#include <QTreeView>
#include <QVBoxLayout>

BatchProcessing_Widget::BatchProcessing_Widget(std::shared_ptr<BatchProcessingState> state,
                                               QWidget * parent)
    : QWidget(parent),
      ui(new Ui::BatchProcessing_Widget),
      m_state(std::move(state)),
      m_mainLayout(nullptr),
      m_splitter(nullptr),
      m_folderGroup(nullptr),
      m_selectFolderButton(nullptr),
      m_folderPathDisplay(nullptr),
      m_treeGroup(nullptr),
      m_treeView(nullptr),
      m_fileSystemModel(nullptr),
      m_jsonGroup(nullptr),
      m_loadJsonButton(nullptr),
      m_loadFolderButton(nullptr),
      m_jsonTextEdit(nullptr),
      m_jsonStatusLabel(nullptr) {
    ui->setupUi(this);
    setupUI();
    setupFileSystemModel();
    connectStateSignals();

    // Initialize UI from state
    if (!m_state->topLevelFolder().isEmpty()) {
        m_folderPathDisplay->setText(m_state->topLevelFolder());
        QModelIndex const rootIndex = m_fileSystemModel->setRootPath(m_state->topLevelFolder());
        m_treeView->setRootIndex(rootIndex);
    }

    if (!m_state->jsonContent().isEmpty()) {
        m_jsonTextEdit->setPlainText(m_state->jsonContent());
    }
}

BatchProcessing_Widget::~BatchProcessing_Widget() {
    delete ui;
}

void BatchProcessing_Widget::openWidget() {
    show();
    raise();
    activateWindow();
}

void BatchProcessing_Widget::connectStateSignals() {
    // Connect state signals to update UI
    connect(m_state.get(), &BatchProcessingState::topLevelFolderChanged,
            this, [this](QString const & folderPath) {
                m_folderPathDisplay->setText(folderPath);
            });

    connect(m_state.get(), &BatchProcessingState::loadCompleted,
            this, &BatchProcessing_Widget::onLoadCompleted);
}

void BatchProcessing_Widget::setupUI() {
    setWindowTitle("Batch Processing");
    setMinimumSize(600, 800);

    // Main layout
    m_mainLayout = new QVBoxLayout(this);
    m_splitter = new QSplitter(Qt::Vertical, this);
    m_mainLayout->addWidget(m_splitter);

    // Folder selection section
    m_folderGroup = new QGroupBox("Top Level Folder Selection", this);
    auto folderLayout = new QVBoxLayout(m_folderGroup);

    auto folderButtonLayout = new QHBoxLayout();
    m_selectFolderButton = new QPushButton("Select Folder...", this);
    m_folderPathDisplay = new QLineEdit(this);
    m_folderPathDisplay->setReadOnly(true);
    m_folderPathDisplay->setPlaceholderText("No folder selected");

    folderButtonLayout->addWidget(m_selectFolderButton);
    folderButtonLayout->addWidget(m_folderPathDisplay, 1);
    folderLayout->addLayout(folderButtonLayout);

    // Tree view section
    m_treeGroup = new QGroupBox("Directory Structure", this);
    auto treeLayout = new QVBoxLayout(m_treeGroup);

    m_treeView = new QTreeView(this);
    m_treeView->setHeaderHidden(true);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setSortingEnabled(true);
    treeLayout->addWidget(m_treeView);

    // JSON configuration section
    m_jsonGroup = new QGroupBox("JSON Configuration", this);
    auto jsonLayout = new QVBoxLayout(m_jsonGroup);

    auto jsonButtonLayout = new QHBoxLayout();
    m_loadJsonButton = new QPushButton("Load JSON File...", this);
    m_jsonStatusLabel = new QLabel("No JSON file loaded", this);
    m_jsonStatusLabel->setStyleSheet("color: gray;");

    jsonButtonLayout->addWidget(m_loadJsonButton);
    jsonButtonLayout->addWidget(m_jsonStatusLabel, 1);
    jsonLayout->addLayout(jsonButtonLayout);

    m_jsonTextEdit = new QTextEdit(this);
    m_jsonTextEdit->setPlaceholderText("JSON configuration will appear here...");
    m_jsonTextEdit->setFont(QFont("Consolas", 9));
    jsonLayout->addWidget(m_jsonTextEdit);

    // Load folder button
    m_loadFolderButton = new QPushButton("Load Folder", this);
    m_loadFolderButton->setEnabled(false);// Initially disabled
    m_loadFolderButton->setStyleSheet("QPushButton { font-weight: bold; }");
    jsonLayout->addWidget(m_loadFolderButton);

    // Add sections to splitter
    m_splitter->addWidget(m_folderGroup);
    m_splitter->addWidget(m_treeGroup);
    m_splitter->addWidget(m_jsonGroup);

    // Set splitter proportions
    m_splitter->setSizes({100, 400, 300});
    m_splitter->setCollapsible(0, false);
    m_splitter->setCollapsible(1, false);
    m_splitter->setCollapsible(2, false);

    // Connect signals
    connect(m_selectFolderButton, &QPushButton::clicked, this, &BatchProcessing_Widget::selectTopLevelFolder);
    connect(m_loadJsonButton, &QPushButton::clicked, this, &BatchProcessing_Widget::loadJsonConfiguration);
    connect(m_loadFolderButton, &QPushButton::clicked, this, &BatchProcessing_Widget::loadFolderWithJson);
    connect(m_jsonTextEdit, &QTextEdit::textChanged, this, &BatchProcessing_Widget::onJsonTextChanged);
}

void BatchProcessing_Widget::setupFileSystemModel() {
    m_fileSystemModel = new QFileSystemModel(this);
    m_fileSystemModel->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    m_fileSystemModel->setRootPath("");

    m_treeView->setModel(m_fileSystemModel);

    // Hide all columns except Name
    for (int i = 1; i < m_fileSystemModel->columnCount(); ++i) {
        m_treeView->hideColumn(i);
    }

    connect(m_treeView, &QTreeView::doubleClicked, this, &BatchProcessing_Widget::onFolderDoubleClicked);
    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &BatchProcessing_Widget::updateLoadFolderButtonState);
}

void BatchProcessing_Widget::selectTopLevelFolder() {
    QString currentFolder = m_state->topLevelFolder();

    QString const folderPath = QFileDialog::getExistingDirectory(
            this,
            "Select Top Level Folder",
            currentFolder.isEmpty() ? QDir::homePath() : currentFolder,
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!folderPath.isEmpty()) {
        // Update state
        m_state->setTopLevelFolder(folderPath);

        // Update UI
        m_folderPathDisplay->setText(folderPath);

        // Set the root path for the tree view
        QModelIndex const rootIndex = m_fileSystemModel->setRootPath(folderPath);
        m_treeView->setRootIndex(rootIndex);

        // Expand the first level
        for (int i = 0; i < m_fileSystemModel->rowCount(rootIndex); ++i) {
            QModelIndex const childIndex = m_fileSystemModel->index(i, 0, rootIndex);
            m_treeView->setExpanded(childIndex, true);
        }

        m_treeGroup->setTitle(QString("Directory Structure - %1").arg(QFileInfo(folderPath).baseName()));
    }
}

void BatchProcessing_Widget::loadJsonConfiguration() {
    QString currentJsonFile = m_state->jsonFilePath();

    QString const jsonFilePath = QFileDialog::getOpenFileName(
            this,
            "Load JSON Configuration",
            currentJsonFile.isEmpty() ? QDir::homePath() : QFileInfo(currentJsonFile).absolutePath(),
            "JSON Files (*.json);;All Files (*)");

    if (!jsonFilePath.isEmpty()) {
        updateJsonDisplay(jsonFilePath);
    }
}

void BatchProcessing_Widget::onFolderDoubleClicked(QModelIndex const & index) {
    if (m_fileSystemModel && index.isValid()) {
        QString folderPath = m_fileSystemModel->filePath(index);

        // Only emit signal for directories
        if (QFileInfo(folderPath).isDir()) {
            emit folderSelected(folderPath);
        }
    }
}

void BatchProcessing_Widget::onJsonTextChanged() {
    // Update state with current JSON content
    m_state->setJsonContent(m_jsonTextEdit->toPlainText());

    validateJsonSyntax();
    updateLoadFolderButtonState();
}

void BatchProcessing_Widget::updateLoadFolderButtonState() {
    // Enable the Load Folder button only if:
    // 1. A folder is selected in the tree view
    // 2. Valid JSON content exists in the text editor

    QString selectedFolder = getCurrentSelectedFolder();
    bool hasValidJson = !m_jsonTextEdit->toPlainText().trimmed().isEmpty();

    // Validate JSON syntax
    if (hasValidJson) {
        QJsonParseError parseError;
        QJsonDocument::fromJson(m_jsonTextEdit->toPlainText().toUtf8(), &parseError);
        hasValidJson = (parseError.error == QJsonParseError::NoError);
    }

    bool canLoad = !selectedFolder.isEmpty() && hasValidJson;
    m_loadFolderButton->setEnabled(canLoad);

    if (canLoad) {
        m_loadFolderButton->setText(QString("Load Folder: %1").arg(QFileInfo(selectedFolder).baseName()));
    } else {
        m_loadFolderButton->setText("Load Folder");
    }
}

void BatchProcessing_Widget::updateJsonDisplay(QString const & jsonFilePath) {
    QFile file(jsonFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", QString("Could not open file: %1").arg(jsonFilePath));
        return;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, "JSON Parse Error",
                             QString("Error parsing JSON file: %1\nAt offset %2")
                                     .arg(parseError.errorString())
                                     .arg(parseError.offset));
        return;
    }

    // Update state
    m_state->setJsonFilePath(jsonFilePath);

    // Display formatted JSON (this will trigger onJsonTextChanged which updates state)
    QString formattedJson = jsonDoc.toJson(QJsonDocument::Indented);
    m_jsonTextEdit->setPlainText(formattedJson);

    m_jsonStatusLabel->setText(QString("Loaded: %1").arg(QFileInfo(jsonFilePath).fileName()));
    m_jsonStatusLabel->setStyleSheet("color: green;");

    validateJsonSyntax();
}

void BatchProcessing_Widget::validateJsonSyntax() {
    QString jsonText = m_jsonTextEdit->toPlainText();

    if (jsonText.trimmed().isEmpty()) {
        m_jsonStatusLabel->setText("No JSON content");
        m_jsonStatusLabel->setStyleSheet("color: gray;");
        return;
    }

    QJsonParseError parseError;
    QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);

    QString currentJsonFile = m_state->jsonFilePath();

    if (parseError.error == QJsonParseError::NoError) {
        if (!currentJsonFile.isEmpty()) {
            m_jsonStatusLabel->setText(QString("Loaded: %1 (Valid JSON)").arg(QFileInfo(currentJsonFile).fileName()));
        } else {
            m_jsonStatusLabel->setText("Valid JSON");
        }
        m_jsonStatusLabel->setStyleSheet("color: green;");
    } else {
        m_jsonStatusLabel->setText(QString("JSON Error: %1").arg(parseError.errorString()));
        m_jsonStatusLabel->setStyleSheet("color: red;");
    }
}

QString BatchProcessing_Widget::getCurrentSelectedFolder() const {
    if (!m_treeView->selectionModel() || !m_fileSystemModel) {
        return QString();
    }

    QModelIndexList selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) {
        return QString();
    }

    QModelIndex selectedIndex = selectedIndexes.first();
    if (!selectedIndex.isValid()) {
        return QString();
    }

    QString folderPath = m_fileSystemModel->filePath(selectedIndex);
    if (QFileInfo(folderPath).isDir()) {
        return folderPath;
    }

    return QString();
}

void BatchProcessing_Widget::loadFolderWithJson() {
    QString selectedFolder = getCurrentSelectedFolder();
    if (selectedFolder.isEmpty()) {
        QMessageBox::warning(this, "No Folder Selected",
                             "Please select a folder from the directory tree first.");
        return;
    }

    QString jsonText = m_jsonTextEdit->toPlainText().trimmed();
    if (jsonText.isEmpty()) {
        QMessageBox::warning(this, "No JSON Configuration",
                             "Please load or enter a JSON configuration first.");
        return;
    }

    // Debug information
    qDebug() << "Selected folder:" << selectedFolder;
    qDebug() << "JSON text length:" << jsonText.length();

    // Validate JSON syntax one more time
    QJsonParseError parseError;
    QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::critical(this, "Invalid JSON",
                              QString("JSON syntax error: %1").arg(parseError.errorString()));
        return;
    }

    // Delegate the actual loading to the state
    // The state will:
    // 1. Reset DataManager
    // 2. Load data via utility function
    // 3. Emit applyDataDisplayConfig signal
    // 4. Emit loadCompleted signal (which we handle in onLoadCompleted)
    m_state->loadFolder(selectedFolder);
}

void BatchProcessing_Widget::onLoadCompleted(BatchProcessingState::LoadResult const & result) {
    if (result.success) {
        QString selectedFolder = getCurrentSelectedFolder();
        QString message = QString("Successfully loaded %1 data items from folder:\n%2")
                                  .arg(result.itemCount)
                                  .arg(selectedFolder);
        QMessageBox::information(this, "Data Loaded", message);

        emit dataLoaded(selectedFolder);
    } else {
        QMessageBox::critical(this, "Loading Error",
                              QString("Error loading data: %1").arg(result.errorMessage));
    }
}
