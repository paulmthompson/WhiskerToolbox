/**
 * @file DataBankViewWidget.cpp
 * @brief Implementation of the memory bank view panel.
 */

#include "DataBankViewWidget.hpp"

#include "ui_DataBankViewWidget.h"

#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"

#include "DeepLearning/storage/DataBank.hpp"
#include "DeepLearning/bindings/DeepLearningBindingData.hpp"

#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

namespace dl::widget {

namespace {

/**
 * @brief Add a centered placeholder label to the preview layout.
 */
QLabel * makePlaceholderLabel(
        QString const & text,
        QWidget * parent,
        bool dashed_border = true) {
    auto * label = new QLabel(text, parent);
    label->setAlignment(Qt::AlignCenter);
    if (dashed_border) {
        label->setStyleSheet(
                QStringLiteral("color: #888; border: 1px dashed #ccc; "
                               "padding: 30px; margin: 10px;"));
    } else {
        label->setStyleSheet(QStringLiteral("color: gray;"));
    }
    return label;
}

}// namespace

DataBankViewWidget::DataBankViewWidget(
        std::shared_ptr<DeepLearningState> state,
        QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)),
      _ui(std::make_unique<Ui::DataBankViewWidget>()) {
    _ui->setupUi(this);

    _ui->previewLayout->addWidget(makePlaceholderLabel(
            tr("Captured DataBank entries will appear here\n"
               "after using \"Capture Current Frame\" in the Memory Bank."),
            _ui->previewContainer));
}

DataBankViewWidget::~DataBankViewWidget() = default;

void DataBankViewWidget::setAssembler(SlotAssembler * assembler) {
    _assembler = assembler;
}

void DataBankViewWidget::_clearPreviewArea() {
    QLayoutItem * child = nullptr;
    while ((child = _ui->previewLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }
}

void DataBankViewWidget::refresh() {
    _clearPreviewArea();

    if (!_assembler) {
        _ui->previewLayout->addWidget(makePlaceholderLabel(
                tr("No assembler connected"),
                _ui->previewContainer,
                /*dashed_border=*/false));
        return;
    }

    auto const & bank = _assembler->dataBank();
    std::vector<std::string> encoded_keys;
    for (auto const & id: bank.ids()) {
        if (bank.getEncoded(id)) {
            encoded_keys.push_back(id);
        }
    }

    if (encoded_keys.empty()) {
        _ui->previewLayout->addWidget(makePlaceholderLabel(
                tr("No DataBank entries with encoded tensors.\n"
                   "Use the Memory Bank panel to capture a frame."),
                _ui->previewContainer));
        return;
    }

    for (auto const & key: encoded_keys) {
        auto * group = new QGroupBox(
                QString::fromStdString(key), _ui->previewContainer);
        auto * layout = new QVBoxLayout(group);

        auto const shape = bank.encodedShape(key);
        auto const range = bank.encodedRange(key);

        QString shape_text = tr("Shape: ");
        for (std::size_t i = 0; i < shape.size(); ++i) {
            if (i > 0) {
                shape_text += QStringLiteral(" \u00D7 ");
            }
            shape_text += QString::number(shape[i]);
        }
        layout->addWidget(new QLabel(shape_text, group));

        layout->addWidget(new QLabel(
                tr("Value range: [%1, %2]")
                        .arg(static_cast<double>(range.first), 0, 'f', 4)
                        .arg(static_cast<double>(range.second), 0, 'f', 4),
                group));

        int captured_frame = -1;
        std::string data_key;
        if (auto const bank_entry = bank.get(key)) {
            captured_frame = bank_entry->metadata.captured_frame;
            data_key = bank_entry->metadata.data_key;
        }

        if (data_key.empty() && _state) {
            for (auto const & frame: _state->memoryFrames()) {
                if (!dl::isStaticFrame(frame)) continue;
                if ((!dl::staticBankEntryId(frame).empty() &&
                     dl::staticBankEntryId(frame) == key) ||
                    dl::resolvedBankEntryId(frame) == key) {
                    if (dl::staticSourceType(frame) == StaticInputSource::DataManager) {
                        data_key = dl::staticDataKey(frame);
                    }
                    break;
                }
            }
        }

        if (!data_key.empty()) {
            layout->addWidget(new QLabel(
                    tr("Source: %1")
                            .arg(QString::fromStdString(data_key)),
                    group));
        }

        if (captured_frame >= 0) {
            layout->addWidget(new QLabel(
                    tr("Captured at frame: %1").arg(captured_frame),
                    group));
        }

        auto * status_label = new QLabel(
                tr("\u2713 Cached and ready for inference"), group);
        status_label->setStyleSheet(
                QStringLiteral("color: green; font-weight: bold;"));
        layout->addWidget(status_label);

        _ui->previewLayout->addWidget(group);
    }

    _ui->previewLayout->addStretch();
}

}// namespace dl::widget
