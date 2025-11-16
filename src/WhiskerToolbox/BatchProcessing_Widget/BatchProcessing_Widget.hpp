#ifndef BATCHPROCESSING_WIDGET_HPP
#define BATCHPROCESSING_WIDGET_HPP

#include "DataManager/DataManagerTypes.hpp" // DataInfo

#include <QFileSystemModel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QWidget>

#include <memory>

class QVBoxLayout;
class QPushButton;
class QLabel;
class QTreeView;
class QTextEdit;
class QSplitter;
class QLineEdit;
class QGroupBox;
class DataManager;
class MainWindow;

QT_BEGIN_NAMESPACE
namespace Ui {
class BatchProcessing_Widget;
}
QT_END_NAMESPACE

class BatchProcessing_Widget : public QWidget {
    Q_OBJECT

public:
    explicit BatchProcessing_Widget(std::shared_ptr<DataManager> dataManager, MainWindow * mainWindow, QWidget * parent = nullptr);
    ~BatchProcessing_Widget();

    void openWidget();

signals:
    void folderSelected(QString const & folderPath);
    void dataLoaded(QString const & folderPath);

private slots:
    void selectTopLevelFolder();
    void loadJsonConfiguration();
    void onFolderDoubleClicked(QModelIndex const & index);
    void onJsonTextChanged();
    void loadFolderWithJson();

private:
    void setupUI();
    void setupFileSystemModel();
    void updateJsonDisplay(QString const & jsonFilePath);
    void validateJsonSyntax();
    void updateLoadFolderButtonState();
    std::vector<DataInfo> loadDataFromJsonContent(QString const & jsonContent, QString const & baseFolderPath);
    QString getCurrentSelectedFolder() const;

    Ui::BatchProcessing_Widget * ui;

    // UI Components
    QVBoxLayout * m_mainLayout;
    QSplitter * m_splitter;

    // Folder selection section
    QGroupBox * m_folderGroup;
    QPushButton * m_selectFolderButton;
    QLineEdit * m_folderPathDisplay;

    // Tree view section
    QGroupBox * m_treeGroup;
    QTreeView * m_treeView;
    QFileSystemModel * m_fileSystemModel;

    // JSON configuration section
    QGroupBox * m_jsonGroup;
    QPushButton * m_loadJsonButton;
    QPushButton * m_loadFolderButton;
    QTextEdit * m_jsonTextEdit;
    QLabel * m_jsonStatusLabel;

    QString m_currentTopLevelFolder;
    QString m_currentJsonFile;
    QJsonDocument m_jsonDocument;
    std::shared_ptr<DataManager> m_dataManager;
    MainWindow * m_mainWindow;
};

#endif// BATCHPROCESSING_WIDGET_HPP
