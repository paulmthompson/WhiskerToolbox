#include "TensorInspector.hpp"

#include "TensorDesigner.hpp"

#include "DataManager/DataManager.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/TensorData.hpp"
#define slots Q_SLOTS

#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "EditorState/SelectionContext.hpp"

#include <QLabel>
#include <QVBoxLayout>

#include <iostream>

TensorInspector::TensorInspector(
        std::shared_ptr<DataManager> data_manager,
        GroupManager * group_manager,
        QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent) {
    _setupDesignerUi();
}

TensorInspector::~TensorInspector() {
    removeCallbacks();
}

void TensorInspector::setActiveKey(std::string const & key) {
    if (_active_key == key && _callback_id != -1) {
        return;
    }

    removeCallbacks();
    _active_key = key;
    _assignCallbacks();

    // If the active key has a LazyColumnTensorStorage tensor, set it on the designer
    if (_designer && !key.empty()) {
        _designer->setTensorKey(key);
    }
}

void TensorInspector::removeCallbacks() {
    remove_callback(dataManager().get(), _active_key, _callback_id);
}

void TensorInspector::updateView() {
    // TensorInspector doesn't maintain its own UI - TensorDataView handles the table
    // This method is called when data changes, but the actual view update
    // is handled by TensorDataView through its own callbacks
}

void TensorInspector::setSelectionContext(SelectionContext * context) {
    if (_designer) {
        _designer->setSelectionContext(context);
    }
}

void TensorInspector::setOperationContext(EditorLib::OperationContext * context) {
    if (_designer) {
        _designer->setOperationContext(context);
    }
}

// =============================================================================
// Private slots
// =============================================================================

void TensorInspector::_onDataChanged() {
    updateView();
}

void TensorInspector::_onTensorCreated(QString const & key) {
    emit tensorCreated(key);
}

// =============================================================================
// Private implementation
// =============================================================================

void TensorInspector::_setupDesignerUi() {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto * title_label = new QLabel(QStringLiteral("<b>Tensor Inspector</b>"), this);
    layout->addWidget(title_label);

    // Embed TensorDesigner
    _designer = new TensorDesigner(dataManager(), this);
    layout->addWidget(_designer);

    connect(_designer, &TensorDesigner::tensorCreated,
            this, &TensorInspector::_onTensorCreated);
}

void TensorInspector::_assignCallbacks() {
    if (!_active_key.empty()) {
        auto tensor_data = dataManager()->getData<TensorData>(_active_key);
        if (tensor_data) {
            _callback_id = dataManager()->addCallbackToData(_active_key, [this]() { _onDataChanged(); });
        } else {
            std::cerr << "TensorInspector: No TensorData found for key '" << _active_key
                      << "' to attach callback." << std::endl;
        }
    }
}
