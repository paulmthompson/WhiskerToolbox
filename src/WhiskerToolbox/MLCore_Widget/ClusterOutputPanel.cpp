#include "ClusterOutputPanel.hpp"

#include "ui_ClusterOutputPanel.h"

#include "pipelines/ClusteringPipeline.hpp"

#include <QHeaderView>
#include <QListWidgetItem>
#include <QString>
#include <QTableWidgetItem>

// =============================================================================
// Construction / destruction
// =============================================================================

ClusterOutputPanel::ClusterOutputPanel(QWidget * parent)
    : QWidget(parent)
    , ui(new Ui::ClusterOutputPanel) {
    ui->setupUi(this);

    // Configure table header sizing
    ui->clusterTable->horizontalHeader()->setStretchLastSection(true);
    ui->clusterTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    ui->clusterTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    ui->clusterTable->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);
    ui->clusterTable->verticalHeader()->setVisible(false);

    connect(ui->outputKeysListWidget, &QListWidget::itemClicked,
            this, &ClusterOutputPanel::_onOutputItemClicked);
    connect(ui->clearButton, &QPushButton::clicked,
            this, &ClusterOutputPanel::_onClearClicked);

    _showNoResultsState();
}

ClusterOutputPanel::~ClusterOutputPanel() {
    delete ui;
}

// =============================================================================
// Public API — display results
// =============================================================================

void ClusterOutputPanel::showClusteringResult(
    MLCore::ClusteringPipelineResult const & result,
    std::string const & algorithm_name) {

    if (!result.success) {
        clearResults();
        return;
    }

    _showResultsState();

    _updateSummary(algorithm_name,
                   result.fitting_observations,
                   result.num_features,
                   result.num_clusters,
                   result.rows_dropped_nan);

    _updateClusterTable(result.cluster_names,
                        result.cluster_sizes,
                        result.assignment_observations);

    _updateNoiseInfo(result.noise_points);

    setOutputKeys(result.interval_keys,
                  result.probability_keys,
                  result.cluster_names,
                  result.putative_group_ids);

    _has_results = true;
}

void ClusterOutputPanel::setOutputKeys(
    std::vector<std::string> const & interval_keys,
    std::vector<std::string> const & probability_keys,
    std::vector<std::string> const & cluster_names,
    std::vector<uint64_t> const & group_ids) {

    ui->outputKeysListWidget->clear();

    // Add putative group entries
    for (std::size_t i = 0; i < group_ids.size(); ++i) {
        std::string const name = (i < cluster_names.size())
                                     ? cluster_names[i]
                                     : "Cluster " + std::to_string(i);
        QString const label = QStringLiteral("Group: %1 (id %2)")
                                  .arg(QString::fromStdString(name))
                                  .arg(group_ids[i]);
        auto * item = new QListWidgetItem(label, ui->outputKeysListWidget);
        // Groups don't have a DataManager key, but store the name for reference
        item->setData(Qt::UserRole, QString::fromStdString(name));
        item->setToolTip(QStringLiteral("Putative entity group"));
    }

    // Add interval series keys
    for (std::size_t i = 0; i < interval_keys.size(); ++i) {
        auto const & key = interval_keys[i];
        QString const label = QStringLiteral("Intervals: %1")
                                  .arg(QString::fromStdString(key));
        auto * item = new QListWidgetItem(label, ui->outputKeysListWidget);
        item->setData(Qt::UserRole, QString::fromStdString(key));
        item->setToolTip(
            QStringLiteral("DigitalIntervalSeries — click to focus in DataViewer"));
    }

    // Add probability series keys
    for (std::size_t i = 0; i < probability_keys.size(); ++i) {
        auto const & key = probability_keys[i];
        QString const label = QStringLiteral("Probabilities: %1")
                                  .arg(QString::fromStdString(key));
        auto * item = new QListWidgetItem(label, ui->outputKeysListWidget);
        item->setData(Qt::UserRole, QString::fromStdString(key));
        item->setToolTip(
            QStringLiteral("AnalogTimeSeries — click to focus in DataViewer"));
    }
}

void ClusterOutputPanel::clearResults() {
    _showNoResultsState();
    _has_results = false;
    emit resultsCleared();
}

bool ClusterOutputPanel::hasResults() const {
    return _has_results;
}

// =============================================================================
// Private slots
// =============================================================================

void ClusterOutputPanel::_onOutputItemClicked(QListWidgetItem * item) {
    if (!item) {
        return;
    }
    QVariant const data = item->data(Qt::UserRole);
    if (data.isValid()) {
        emit outputKeyClicked(data.toString());
    }
}

void ClusterOutputPanel::_onClearClicked() {
    clearResults();
}

// =============================================================================
// Private helpers
// =============================================================================

void ClusterOutputPanel::_showNoResultsState() {
    ui->stackedWidget->setCurrentWidget(ui->noResultsPage);
    ui->outputKeysListWidget->clear();
    ui->clusterTable->setRowCount(0);
    ui->noiseLabel->clear();
}

void ClusterOutputPanel::_showResultsState() {
    ui->stackedWidget->setCurrentWidget(ui->resultsPage);
}

void ClusterOutputPanel::_updateSummary(
    std::string const & algorithm_name,
    std::size_t fitting_observations,
    std::size_t num_features,
    std::size_t num_clusters,
    std::size_t rows_dropped_nan) {

    ui->summaryLabel->setText(
        QStringLiteral("Algorithm: %1 — %2 clusters")
            .arg(QString::fromStdString(algorithm_name))
            .arg(num_clusters));

    QString fitting_text = QStringLiteral("Fitting: %1 observations, %2 features")
                               .arg(fitting_observations)
                               .arg(num_features);
    if (rows_dropped_nan > 0) {
        fitting_text += QStringLiteral(" (%1 rows dropped due to NaN)")
                            .arg(rows_dropped_nan);
    }
    ui->fittingInfoLabel->setText(fitting_text);
}

void ClusterOutputPanel::_updateClusterTable(
    std::vector<std::string> const & cluster_names,
    std::vector<std::size_t> const & cluster_sizes,
    std::size_t total_assigned) {

    std::size_t const num_clusters = cluster_names.size();
    ui->clusterTable->setRowCount(static_cast<int>(num_clusters));

    double const total = (total_assigned > 0)
                             ? static_cast<double>(total_assigned)
                             : 1.0;

    for (std::size_t i = 0; i < num_clusters; ++i) {
        int const row = static_cast<int>(i);

        // Cluster name
        auto * name_item = new QTableWidgetItem(
            QString::fromStdString(cluster_names[i]));
        name_item->setFlags(Qt::ItemIsEnabled);
        ui->clusterTable->setItem(row, 0, name_item);

        // Count
        std::size_t const count = (i < cluster_sizes.size()) ? cluster_sizes[i] : 0;
        auto * count_item = new QTableWidgetItem(
            QString::number(static_cast<qulonglong>(count)));
        count_item->setFlags(Qt::ItemIsEnabled);
        count_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ui->clusterTable->setItem(row, 1, count_item);

        // Fraction
        double const fraction = static_cast<double>(count) / total * 100.0;
        auto * fraction_item = new QTableWidgetItem(
            QStringLiteral("%1%").arg(fraction, 0, 'f', 1));
        fraction_item->setFlags(Qt::ItemIsEnabled);
        fraction_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ui->clusterTable->setItem(row, 2, fraction_item);
    }

    ui->clusterTable->resizeColumnsToContents();
}

void ClusterOutputPanel::_updateNoiseInfo(std::size_t noise_points) {
    if (noise_points > 0) {
        ui->noiseLabel->setText(
            QStringLiteral("Noise points (unassigned): %1").arg(
                static_cast<qulonglong>(noise_points)));
        ui->noiseLabel->setVisible(true);
    } else {
        ui->noiseLabel->clear();
        ui->noiseLabel->setVisible(false);
    }
}
