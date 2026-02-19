#include "MLCoreWidget.hpp"

#include "MLCoreWidgetState.hpp"

#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

MLCoreWidget::MLCoreWidget(std::shared_ptr<MLCoreWidgetState> state,
                           std::shared_ptr<DataManager> data_manager,
                           SelectionContext * selection_context,
                           QWidget * parent)
    : QWidget(parent)
    , _state(std::move(state))
    , _data_manager(std::move(data_manager))
    , _selection_context(selection_context) {
    _setupUi();
    _connectSignals();
}

void MLCoreWidget::_setupUi() {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto * tabs = new QTabWidget(this);

    // Classification tab — stub placeholder
    auto * classification_tab = new QWidget();
    auto * classification_layout = new QVBoxLayout(classification_tab);
    auto * classification_label = new QLabel(
        QStringLiteral("Classification workflow panels will be added here.\n\n"
                       "Planned sub-panels:\n"
                       "• Feature Selection\n"
                       "• Training Region\n"
                       "• Labels\n"
                       "• Model Configuration\n"
                       "• Prediction\n"
                       "• Results"),
        classification_tab);
    classification_label->setWordWrap(true);
    classification_label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    classification_layout->addWidget(classification_label);
    classification_layout->addStretch();

    // Clustering tab — stub placeholder
    auto * clustering_tab = new QWidget();
    auto * clustering_layout = new QVBoxLayout(clustering_tab);
    auto * clustering_label = new QLabel(
        QStringLiteral("Clustering workflow panels will be added here.\n\n"
                       "Planned sub-panels:\n"
                       "• Data Source\n"
                       "• Algorithm Configuration\n"
                       "• Output"),
        clustering_tab);
    clustering_label->setWordWrap(true);
    clustering_label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    clustering_layout->addWidget(clustering_label);
    clustering_layout->addStretch();

    tabs->addTab(classification_tab, QStringLiteral("Classification"));
    tabs->addTab(clustering_tab, QStringLiteral("Clustering"));

    // Restore active tab from state
    if (_state) {
        tabs->setCurrentIndex(_state->activeTab());
    }

    // Track tab changes in state
    connect(tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (_state) {
            _state->setActiveTab(index);
        }
    });

    layout->addWidget(tabs);
}

void MLCoreWidget::_connectSignals() {
    // Future: connect SelectionContext::dataFocusChanged for passive awareness
    // Future: connect state signals to update UI panels
}
