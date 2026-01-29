#include "ImageDataView.hpp"

#include "ImageTableModel.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Media/Image_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"

#include <QHeaderView>
#include <QTableView>
#include <QVBoxLayout>

ImageDataView::ImageDataView(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : BaseDataView(std::move(data_manager), parent)
    , _table_model(new ImageTableModel(this)) {
    _setupUi();
    _connectSignals();
}

ImageDataView::~ImageDataView() {
    removeCallbacks();
}

void ImageDataView::setActiveKey(std::string const & key) {
    if (_active_key == key && dataManager()->getData<MediaData>(key)) {
        return;
    }

    removeCallbacks();
    _active_key = key;

    auto media_data = dataManager()->getData<MediaData>(_active_key);
    if (media_data) {
        auto image_data = dynamic_cast<ImageData *>(media_data.get());
        _table_model->setImages(image_data);
        _callback_id = media_data->addObserver([this]() { _onDataChanged(); });
    } else {
        _table_model->setImages(nullptr);
    }
}

void ImageDataView::removeCallbacks() {
    remove_callback(dataManager().get(), _active_key, _callback_id);
}

void ImageDataView::updateView() {
    if (!_active_key.empty() && _table_model) {
        auto media_data = dataManager()->getData<MediaData>(_active_key);
        auto image_data = media_data ? dynamic_cast<ImageData *>(media_data.get()) : nullptr;
        _table_model->setImages(image_data);
    }
}

std::vector<int> ImageDataView::getSelectedFrames() const {
    std::vector<int> frames;
    
    if (!_table_view || !_table_model) {
        return frames;
    }

    auto const selection = _table_view->selectionModel()->selectedRows();
    frames.reserve(static_cast<size_t>(selection.size()));
    
    for (auto const & index : selection) {
        int frame = _table_model->getFrameForRow(index.row());
        if (frame >= 0) {
            frames.push_back(frame);
        }
    }

    return frames;
}

void ImageDataView::_setupUi() {
    _layout = new QVBoxLayout(this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(0);

    _table_view = new QTableView(this);
    _table_view->setModel(_table_model);
    _table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _table_view->setAlternatingRowColors(true);
    _table_view->setSortingEnabled(true);
    _table_view->horizontalHeader()->setStretchLastSection(true);

    _layout->addWidget(_table_view);
}

void ImageDataView::_connectSignals() {
    connect(_table_view, &QTableView::doubleClicked,
            this, &ImageDataView::_handleTableViewDoubleClicked);
}

void ImageDataView::_handleTableViewDoubleClicked(QModelIndex const & index) {
    if (index.isValid() && _table_model) {

        // Using data key, we can get the TimeFrame from the data manager
        auto tf = dataManager()->getTime(TimeKey(_active_key));

        int frame = _table_model->getFrameForRow(index.row());
        if (frame >= 0) {
            emit frameSelected(TimePosition(frame, tf));
        }
    }
}

void ImageDataView::_onDataChanged() {
    updateView();
}
