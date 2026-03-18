#include "ResultsPanel.hpp"

#include "ui_ResultsPanel.h"

#include "GroupManagementWidget/GroupManager.hpp"
#include "metrics/ClassificationMetrics.hpp"
#include "output/PredictionWriter.hpp"
#include "pipelines/ClassificationPipeline.hpp"

#include <QListWidgetItem>
#include <QPixmap>
#include <QString>
#include <armadillo>

#include <algorithm>
#include <iomanip>
#include <sstream>

// =============================================================================
// Construction / destruction
// =============================================================================

ResultsPanel::ResultsPanel(GroupManager * group_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::ResultsPanel),
      _group_manager(group_manager) {
    ui->setupUi(this);

    connect(ui->outputKeysListWidget, &QListWidget::itemClicked,
            this, &ResultsPanel::_onOutputItemClicked);
    connect(ui->clearButton, &QPushButton::clicked,
            this, &ResultsPanel::_onClearClicked);

    _showNoResultsState();
}

ResultsPanel::~ResultsPanel() {
    delete ui;
}

// =============================================================================
// Public API — display results
// =============================================================================

void ResultsPanel::showClassificationResult(
        MLCore::ClassificationPipelineResult const & result,
        std::string const & model_name) {

    if (!result.success) {
        clearResults();
        return;
    }

    _showResultsState();

    // Model & training summary
    _updateModelSummary(model_name,
                        result.training_observations,
                        result.num_features,
                        result.num_classes,
                        result.was_balanced);

    // Metrics display
    if (result.binary_train_metrics.has_value()) {
        _updateBinaryMetricsDisplay(result.binary_train_metrics.value());
    } else if (result.multi_class_train_metrics.has_value()) {
        _updateMultiClassMetricsDisplay(result.multi_class_train_metrics.value(),
                                        result.class_names);
    }

    // Output keys
    if (result.writer_result.has_value()) {
        setOutputKeys(result.writer_result->interval_keys,
                      result.writer_result->probability_keys,
                      result.writer_result->class_names,
                      result.writer_result->putative_group_ids);
    }

    _has_results = true;
}

void ResultsPanel::showBinaryMetrics(
        MLCore::BinaryClassificationMetrics const & metrics,
        std::string const & model_name,
        std::size_t training_observations,
        std::size_t num_features) {

    _showResultsState();
    _updateModelSummary(model_name, training_observations, num_features, 2, false);
    _updateBinaryMetricsDisplay(metrics);
    _has_results = true;
}

void ResultsPanel::showMultiClassMetrics(
        MLCore::MultiClassMetrics const & metrics,
        std::vector<std::string> const & class_names,
        std::string const & model_name,
        std::size_t training_observations,
        std::size_t num_features) {

    _showResultsState();
    _updateModelSummary(model_name, training_observations, num_features,
                        metrics.num_classes, false);
    _updateMultiClassMetricsDisplay(metrics, class_names);
    _has_results = true;
}

void ResultsPanel::setOutputKeys(
        std::vector<std::string> const & interval_keys,
        std::vector<std::string> const & probability_keys,
        std::vector<std::string> const & class_names,
        std::vector<uint64_t> const & putative_group_ids) {

    ui->outputKeysListWidget->clear();
    _last_putative_group_ids = putative_group_ids;

    // Add putative group entries with color swatches (if groups were created)
    for (std::size_t i = 0; i < putative_group_ids.size(); ++i) {
        std::string const name = (i < class_names.size())
                                         ? class_names[i]
                                         : "Class " + std::to_string(i);
        QString const label = QStringLiteral("Group: %1 (id %2)")
                                      .arg(QString::fromStdString(name))
                                      .arg(putative_group_ids[i]);
        auto * item = new QListWidgetItem(label, ui->outputKeysListWidget);
        item->setData(Qt::UserRole, QString::fromStdString(name));
        item->setData(Qt::UserRole + 1, static_cast<int>(putative_group_ids[i]));
        item->setToolTip(QStringLiteral("Putative entity group — click to select entities"));

        // Show color swatch from GroupManager if available
        if (_group_manager) {
            auto group = _group_manager->getGroup(static_cast<int>(putative_group_ids[i]));
            if (group.has_value()) {
                QPixmap swatch(12, 12);
                swatch.fill(group->color);
                item->setIcon(QIcon(swatch));
            }
        }
    }

    // Add interval series keys
    for (const auto & key : interval_keys) {
        QString const label = QStringLiteral("Intervals: %1").arg(QString::fromStdString(key));
        auto * item = new QListWidgetItem(label, ui->outputKeysListWidget);
        item->setData(Qt::UserRole, QString::fromStdString(key));
        item->setToolTip(QStringLiteral("DigitalIntervalSeries — click to focus in DataViewer"));
    }

    // Add probability series keys
    for (const auto & key : probability_keys) {
        QString const label = QStringLiteral("Probabilities: %1").arg(QString::fromStdString(key));
        auto * item = new QListWidgetItem(label, ui->outputKeysListWidget);
        item->setData(Qt::UserRole, QString::fromStdString(key));
        item->setToolTip(QStringLiteral("AnalogTimeSeries — click to focus in DataViewer"));
    }
}

void ResultsPanel::clearResults() {
    _showNoResultsState();
    _last_putative_group_ids.clear();
    _has_results = false;
    ui->transitionMatrixHeaderLabel->setVisible(false);
    ui->transitionMatrixLabel->setVisible(false);
    emit resultsCleared();
}

bool ResultsPanel::hasResults() const {
    return _has_results;
}

void ResultsPanel::showTransitionMatrix(arma::mat const & transition_matrix,
                                        std::vector<std::string> const & class_names) {
    if (transition_matrix.empty()) {
        ui->transitionMatrixHeaderLabel->setVisible(false);
        ui->transitionMatrixLabel->setVisible(false);
        return;
    }

    ui->transitionMatrixHeaderLabel->setVisible(true);
    ui->transitionMatrixLabel->setVisible(true);
    ui->transitionMatrixLabel->setText(_formatTransitionMatrix(transition_matrix, class_names));
}

// =============================================================================
// Private slots
// =============================================================================

void ResultsPanel::_onOutputItemClicked(QListWidgetItem * item) {
    if (!item) {
        return;
    }

    // Check if this is a group entry (stores group_id in UserRole+1)
    QVariant const group_data = item->data(Qt::UserRole + 1);
    if (group_data.isValid()) {
        emit groupClicked(group_data.toInt());
        return;
    }

    // Otherwise it's a data key entry
    QVariant const data = item->data(Qt::UserRole);
    if (data.isValid()) {
        emit outputKeyClicked(data.toString());
    }
}

void ResultsPanel::_onClearClicked() {
    clearResults();
}

// =============================================================================
// Private helpers
// =============================================================================

void ResultsPanel::_showNoResultsState() {
    ui->stackedWidget->setCurrentWidget(ui->noResultsPage);
    ui->outputKeysListWidget->clear();
}

void ResultsPanel::_showResultsState() {
    ui->stackedWidget->setCurrentWidget(ui->resultsPage);
}

void ResultsPanel::_updateBinaryMetricsDisplay(
        MLCore::BinaryClassificationMetrics const & metrics) {

    ui->metricsHeaderLabel->setText(QStringLiteral("Binary Classification Metrics"));

    ui->accuracyValueLabel->setText(
            QStringLiteral("%1%").arg(metrics.accuracy * 100.0, 0, 'f', 2));
    ui->sensitivityValueLabel->setText(
            QStringLiteral("%1%").arg(metrics.sensitivity * 100.0, 0, 'f', 2));
    ui->specificityValueLabel->setText(
            QStringLiteral("%1%").arg(metrics.specificity * 100.0, 0, 'f', 2));
    ui->diceScoreValueLabel->setText(
            QStringLiteral("%1%").arg(metrics.dice_score * 100.0, 0, 'f', 2));

    ui->confusionMatrixLabel->setText(_formatConfusionMatrix(metrics));
}

void ResultsPanel::_updateMultiClassMetricsDisplay(
        MLCore::MultiClassMetrics const & metrics,
        std::vector<std::string> const & class_names) {

    ui->metricsHeaderLabel->setText(
            QStringLiteral("Multi-Class Metrics (%1 classes)").arg(metrics.num_classes));

    ui->accuracyValueLabel->setText(
            QStringLiteral("%1%").arg(metrics.overall_accuracy * 100.0, 0, 'f', 2));

    // For multi-class, show macro-averaged precision/recall/F1
    if (!metrics.per_class_recall.empty()) {
        double avg_recall = 0.0;
        for (double const r: metrics.per_class_recall) {
            avg_recall += r;
        }
        avg_recall /= static_cast<double>(metrics.per_class_recall.size());
        ui->sensitivityValueLabel->setText(
                QStringLiteral("%1% (macro avg)").arg(avg_recall * 100.0, 0, 'f', 2));
    } else {
        ui->sensitivityValueLabel->setText(QStringLiteral("--"));
    }

    if (!metrics.per_class_precision.empty()) {
        double avg_precision = 0.0;
        for (double const p: metrics.per_class_precision) {
            avg_precision += p;
        }
        avg_precision /= static_cast<double>(metrics.per_class_precision.size());
        ui->specificityValueLabel->setText(
                QStringLiteral("%1% (macro avg)").arg(avg_precision * 100.0, 0, 'f', 2));
        // Re-label for multi-class: specificity becomes precision
        ui->specificityTextLabel->setText(QStringLiteral("Precision (macro):"));
    } else {
        ui->specificityValueLabel->setText(QStringLiteral("--"));
    }

    if (!metrics.per_class_f1.empty()) {
        double avg_f1 = 0.0;
        for (double const f: metrics.per_class_f1) {
            avg_f1 += f;
        }
        avg_f1 /= static_cast<double>(metrics.per_class_f1.size());
        ui->diceScoreValueLabel->setText(
                QStringLiteral("%1% (macro avg)").arg(avg_f1 * 100.0, 0, 'f', 2));
    } else {
        ui->diceScoreValueLabel->setText(QStringLiteral("--"));
    }

    ui->confusionMatrixLabel->setText(
            _formatMultiClassConfusionMatrix(metrics, class_names));
}

void ResultsPanel::_updateModelSummary(
        std::string const & model_name,
        std::size_t training_observations,
        std::size_t num_features,
        std::size_t num_classes,
        bool was_balanced) {

    QString summary = QStringLiteral("Model: %1").arg(QString::fromStdString(model_name));
    if (was_balanced) {
        summary += QStringLiteral(" (balanced)");
    }
    ui->modelSummaryLabel->setText(summary);

    ui->trainingSummaryLabel->setText(
            QStringLiteral("Training: %1 observations, %2 features, %3 classes")
                    .arg(training_observations)
                    .arg(num_features)
                    .arg(num_classes));
}

// =============================================================================
// Static formatting helpers
// =============================================================================

QString ResultsPanel::_formatConfusionMatrix(
        MLCore::BinaryClassificationMetrics const & metrics) {

    std::size_t const max_val = std::max({metrics.true_positives, metrics.true_negatives,
                                          metrics.false_positives, metrics.false_negatives});
    int field_width = static_cast<int>(std::to_string(max_val).length()) + 1;
    field_width = std::max(field_width, 4);

    std::ostringstream oss;
    oss << "           Predicted\n";
    oss << "         |   0  |   1  |\n";
    oss << "       --+------+------+\n";
    oss << "Actual 0 |" << std::setw(field_width) << metrics.true_negatives
        << " |" << std::setw(field_width) << metrics.false_positives << " |\n";
    oss << "       1 |" << std::setw(field_width) << metrics.false_negatives
        << " |" << std::setw(field_width) << metrics.true_positives << " |\n";
    oss << "\n";
    oss << "Total samples: " << metrics.getTotalPredictions();

    return QString::fromStdString(oss.str());
}

QString ResultsPanel::_formatMultiClassConfusionMatrix(
        MLCore::MultiClassMetrics const & metrics,
        std::vector<std::string> const & class_names) {

    if (!metrics.isValid()) {
        return QStringLiteral("No confusion matrix available");
    }

    std::size_t const n = metrics.num_classes;
    std::ostringstream oss;

    // Find max value for column width
    std::size_t max_val = 0;
    for (std::size_t r = 0; r < n; ++r) {
        for (std::size_t c = 0; c < n; ++c) {
            if (r < metrics.confusion_matrix.n_rows && c < metrics.confusion_matrix.n_cols) {
                max_val = std::max(max_val, static_cast<std::size_t>(metrics.confusion_matrix(r, c)));
            }
        }
    }
    int const fw = std::max(static_cast<int>(std::to_string(max_val).length()) + 1, 4);

    oss << "Predicted ->\n";

    // Header with class indices
    oss << "     ";
    for (std::size_t c = 0; c < n; ++c) {
        oss << " |" << std::setw(fw) << c;
    }
    oss << " |\n";

    // Separator
    oss << "-----";
    for (std::size_t c = 0; c < n; ++c) {
        oss << "-+" << std::string(static_cast<std::size_t>(fw), '-');
    }
    oss << "-+\n";

    // Rows
    for (std::size_t r = 0; r < n; ++r) {
        std::string row_label = (r < class_names.size()) ? class_names[r] : std::to_string(r);
        if (row_label.length() > 4) {
            row_label = row_label.substr(0, 4);
        }
        oss << std::setw(4) << row_label << " ";
        for (std::size_t c = 0; c < n; ++c) {
            std::size_t val = 0;
            if (r < metrics.confusion_matrix.n_rows && c < metrics.confusion_matrix.n_cols) {
                val = static_cast<std::size_t>(metrics.confusion_matrix(r, c));
            }
            oss << " |" << std::setw(fw) << val;
        }
        oss << " |\n";
    }

    // Per-class metrics summary
    oss << "\n";
    for (std::size_t i = 0; i < n; ++i) {
        std::string const name = (i < class_names.size()) ? class_names[i] : std::to_string(i);
        double const prec = (i < metrics.per_class_precision.size()) ? metrics.per_class_precision[i] : 0.0;
        double const rec = (i < metrics.per_class_recall.size()) ? metrics.per_class_recall[i] : 0.0;
        double const f1 = (i < metrics.per_class_f1.size()) ? metrics.per_class_f1[i] : 0.0;
        oss << name << ": P=" << std::fixed << std::setprecision(2) << (prec * 100.0) << "%"
            << " R=" << (rec * 100.0) << "%"
            << " F1=" << (f1 * 100.0) << "%\n";
    }

    return QString::fromStdString(oss.str());
}

QString ResultsPanel::_formatTransitionMatrix(
        arma::mat const & transition_matrix,
        std::vector<std::string> const & class_names) {

    std::size_t const n = transition_matrix.n_rows;
    std::ostringstream oss;

    int constexpr fw = 7;// field width for probability values

    oss << "P(next state | current state)\n\n";

    // Header row
    oss << "     ";
    for (std::size_t c = 0; c < n; ++c) {
        std::string name = (c < class_names.size()) ? class_names[c] : "S" + std::to_string(c);
        if (name.length() > 5) {
            name = name.substr(0, 5);
        }
        oss << " |" << std::setw(fw) << name;
    }
    oss << " |\n";

    // Separator
    oss << "-----";
    for (std::size_t c = 0; c < n; ++c) {
        oss << "-+" << std::string(static_cast<std::size_t>(fw), '-');
    }
    oss << "-+\n";

    // Rows
    for (std::size_t r = 0; r < n; ++r) {
        std::string row_label = (r < class_names.size()) ? class_names[r] : "S" + std::to_string(r);
        if (row_label.length() > 4) {
            row_label = row_label.substr(0, 4);
        }
        oss << std::setw(4) << row_label << " ";
        for (std::size_t c = 0; c < n; ++c) {
            oss << " |" << std::setw(fw) << std::fixed << std::setprecision(4)
                << transition_matrix(r, c);
        }
        oss << " |\n";
    }

    return QString::fromStdString(oss.str());
}
