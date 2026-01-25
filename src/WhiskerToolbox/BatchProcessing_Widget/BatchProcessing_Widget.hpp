#ifndef BATCHPROCESSING_WIDGET_HPP
#define BATCHPROCESSING_WIDGET_HPP

#include "BatchProcessingState.hpp"
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

QT_BEGIN_NAMESPACE
namespace Ui {
class BatchProcessing_Widget;
}
QT_END_NAMESPACE

/**
 * @brief Widget for batch processing of data across multiple folders
 * 
 * BatchProcessing_Widget provides a UI for:
 * - Selecting a top-level folder containing data
 * - Loading/editing JSON configuration for data loading
 * - Navigating subfolders and loading data from them
 * 
 * The widget is backed by BatchProcessingState which handles the actual
 * loading logic and communicates with DataManager and EditorRegistry.
 * 
 * @see BatchProcessingState for the underlying state
 */
class BatchProcessing_Widget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a BatchProcessing_Widget
     * @param state The BatchProcessingState backing this widget
     * @param parent Parent widget
     */
    explicit BatchProcessing_Widget(std::shared_ptr<BatchProcessingState> state, QWidget * parent = nullptr);
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
    void onLoadCompleted(BatchProcessingState::LoadResult const & result);

private:
    void setupUI();
    void setupFileSystemModel();
    void connectStateSignals();
    void updateJsonDisplay(QString const & jsonFilePath);
    void validateJsonSyntax();
    void updateLoadFolderButtonState();
    QString getCurrentSelectedFolder() const;

    Ui::BatchProcessing_Widget * ui;

    // State
    std::shared_ptr<BatchProcessingState> m_state;

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
};

#endif// BATCHPROCESSING_WIDGET_HPP
