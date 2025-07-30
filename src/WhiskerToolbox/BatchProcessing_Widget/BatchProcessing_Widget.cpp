#include "BatchProcessing_Widget.hpp"
#include "ui_BatchProcessing_Widget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTreeView>
#include <QTextEdit>
#include <QSplitter>
#include <QLineEdit>
#include <QGroupBox>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QMessageBox>
#include <QApplication>
#include <QDir>
#include <QFont>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QRegularExpression>

BatchProcessing_Widget::BatchProcessing_Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::BatchProcessing_Widget)
    , m_mainLayout(nullptr)
    , m_splitter(nullptr)
    , m_folderGroup(nullptr)
    , m_selectFolderButton(nullptr)
    , m_folderPathDisplay(nullptr)
    , m_treeGroup(nullptr)
    , m_treeView(nullptr)
    , m_fileSystemModel(nullptr)
    , m_jsonGroup(nullptr)
    , m_loadJsonButton(nullptr)
    , m_jsonTextEdit(nullptr)
    , m_jsonStatusLabel(nullptr)
{
    ui->setupUi(this);
    setupUI();
    setupFileSystemModel();
}

BatchProcessing_Widget::~BatchProcessing_Widget()
{
    delete ui;
}

void BatchProcessing_Widget::openWidget()
{
    show();
    raise();
    activateWindow();
}

void BatchProcessing_Widget::setupUI()
{
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
    connect(m_jsonTextEdit, &QTextEdit::textChanged, this, &BatchProcessing_Widget::onJsonTextChanged);
}

void BatchProcessing_Widget::setupFileSystemModel()
{
    m_fileSystemModel = new QFileSystemModel(this);
    m_fileSystemModel->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    m_fileSystemModel->setRootPath("");
    
    m_treeView->setModel(m_fileSystemModel);
    
    // Hide all columns except Name
    for (int i = 1; i < m_fileSystemModel->columnCount(); ++i) {
        m_treeView->hideColumn(i);
    }
    
    connect(m_treeView, &QTreeView::doubleClicked, this, &BatchProcessing_Widget::onFolderDoubleClicked);
}

void BatchProcessing_Widget::selectTopLevelFolder()
{
    QString folderPath = QFileDialog::getExistingDirectory(
        this,
        "Select Top Level Folder",
        m_currentTopLevelFolder.isEmpty() ? QDir::homePath() : m_currentTopLevelFolder,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!folderPath.isEmpty()) {
        m_currentTopLevelFolder = folderPath;
        m_folderPathDisplay->setText(folderPath);
        
        // Set the root path for the tree view
        QModelIndex rootIndex = m_fileSystemModel->setRootPath(folderPath);
        m_treeView->setRootIndex(rootIndex);
        
        // Expand the first level
        for (int i = 0; i < m_fileSystemModel->rowCount(rootIndex); ++i) {
            QModelIndex childIndex = m_fileSystemModel->index(i, 0, rootIndex);
            m_treeView->setExpanded(childIndex, true);
        }
        
        m_treeGroup->setTitle(QString("Directory Structure - %1").arg(QFileInfo(folderPath).baseName()));
    }
}

void BatchProcessing_Widget::loadJsonConfiguration()
{
    QString jsonFilePath = QFileDialog::getOpenFileName(
        this,
        "Load JSON Configuration",
        m_currentJsonFile.isEmpty() ? QDir::homePath() : QFileInfo(m_currentJsonFile).absolutePath(),
        "JSON Files (*.json);;All Files (*)"
    );
    
    if (!jsonFilePath.isEmpty()) {
        updateJsonDisplay(jsonFilePath);
    }
}

void BatchProcessing_Widget::onFolderDoubleClicked(const QModelIndex& index)
{
    if (m_fileSystemModel && index.isValid()) {
        QString folderPath = m_fileSystemModel->filePath(index);
        
        // Only emit signal for directories
        if (QFileInfo(folderPath).isDir()) {
            emit folderSelected(folderPath);
        }
    }
}

void BatchProcessing_Widget::onJsonTextChanged()
{
    validateJsonSyntax();
}

void BatchProcessing_Widget::updateJsonDisplay(const QString& jsonFilePath)
{
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
    
    m_currentJsonFile = jsonFilePath;
    m_jsonDocument = jsonDoc;
    
    // Display formatted JSON
    QString formattedJson = jsonDoc.toJson(QJsonDocument::Indented);
    m_jsonTextEdit->setPlainText(formattedJson);
    
    m_jsonStatusLabel->setText(QString("Loaded: %1").arg(QFileInfo(jsonFilePath).fileName()));
    m_jsonStatusLabel->setStyleSheet("color: green;");
    
    validateJsonSyntax();
}

void BatchProcessing_Widget::validateJsonSyntax()
{
    QString jsonText = m_jsonTextEdit->toPlainText();
    
    if (jsonText.trimmed().isEmpty()) {
        m_jsonStatusLabel->setText("No JSON content");
        m_jsonStatusLabel->setStyleSheet("color: gray;");
        return;
    }
    
    QJsonParseError parseError;
    QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);
    
    if (parseError.error == QJsonParseError::NoError) {
        if (!m_currentJsonFile.isEmpty()) {
            m_jsonStatusLabel->setText(QString("Loaded: %1 (Valid JSON)").arg(QFileInfo(m_currentJsonFile).fileName()));
        } else {
            m_jsonStatusLabel->setText("Valid JSON");
        }
        m_jsonStatusLabel->setStyleSheet("color: green;");
    } else {
        m_jsonStatusLabel->setText(QString("JSON Error: %1").arg(parseError.errorString()));
        m_jsonStatusLabel->setStyleSheet("color: red;");
    }
}
