/**
 * @file PlotWidgetCommonTests.test.cpp
 * @brief Instantiation of common DataManager integration tests for plot widgets
 *
 * This file defines Traits structs for LinePlotPropertiesWidget,
 * EventPlotPropertiesWidget, and ACFPropertiesWidget and instantiates the
 * shared test suite from PlotWidgetCommonTests.hpp.
 *
 * To add a new widget, define a Traits struct (see the header for the
 * required interface) and call REGISTER_PLOT_WIDGET_COMMON_TESTS(MyTraits).
 */

#include "PlotWidgetCommonTests.hpp"

// ── LinePlot ─────────────────────────────────────────────────────────────────

#include "Plots/LinePlotWidget/UI/LinePlotPropertiesWidget.hpp"
#include "Plots/LinePlotWidget/Core/LinePlotState.hpp"
#include "Plots/Common/PlotAlignmentWidget/UI/PlotAlignmentWidget.hpp"

#include <QComboBox>

struct LinePlotTraits {
    using State = LinePlotState;
    using PropertiesWidget = LinePlotPropertiesWidget;

    static constexpr char const * name = "LinePlotProperties";

    static std::shared_ptr<State> createState() { return std::make_shared<State>(); }

    static std::unique_ptr<PropertiesWidget> createWidget(
        std::shared_ptr<State> state,
        std::shared_ptr<DataManager> dm)
    {
        return std::make_unique<PropertiesWidget>(std::move(state), std::move(dm));
    }

    static QComboBox * findCombo(PropertiesWidget * w)
    {
        auto * alignment = w->findChild<PlotAlignmentWidget *>();
        return alignment ? alignment->findChild<QComboBox *>("alignment_event_combo") : nullptr;
    }

    static bool comboContainsKey(PropertiesWidget * w, std::string const & key)
    {
        auto * combo = findCombo(w);
        if (!combo) return false;
        QString target = QString::fromStdString(key);
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemText(i) == target) return true;
        }
        return false;
    }

    static int comboCount(PropertiesWidget * w)
    {
        auto * combo = findCombo(w);
        return combo ? combo->count() : 0;
    }
};

REGISTER_PLOT_WIDGET_COMMON_TESTS(LinePlotTraits)

// ── EventPlot ────────────────────────────────────────────────────────────────

#include "Plots/EventPlotWidget/UI/EventPlotPropertiesWidget.hpp"
#include "Plots/EventPlotWidget/Core/EventPlotState.hpp"

struct EventPlotTraits {
    using State = EventPlotState;
    using PropertiesWidget = EventPlotPropertiesWidget;

    static constexpr char const * name = "EventPlotProperties";

    static std::shared_ptr<State> createState() { return std::make_shared<State>(); }

    static std::unique_ptr<PropertiesWidget> createWidget(
        std::shared_ptr<State> state,
        std::shared_ptr<DataManager> dm)
    {
        return std::make_unique<PropertiesWidget>(std::move(state), std::move(dm));
    }

    // EventPlot has both an "add_event_combo" (DigitalEventSeries only) and
    // an alignment combo. We test the "add_event_combo" since that is the
    // widget-specific one; the alignment combo is tested via LinePlot traits.
    static QComboBox * findCombo(PropertiesWidget * w)
    {
        return w->findChild<QComboBox *>("add_event_combo");
    }

    static bool comboContainsKey(PropertiesWidget * w, std::string const & key)
    {
        auto * combo = findCombo(w);
        if (!combo) return false;
        QString target = QString::fromStdString(key);
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemText(i) == target) return true;
        }
        return false;
    }

    static int comboCount(PropertiesWidget * w)
    {
        auto * combo = findCombo(w);
        return combo ? combo->count() : 0;
    }
};

REGISTER_PLOT_WIDGET_COMMON_TESTS(EventPlotTraits)

// ── ACF ──────────────────────────────────────────────────────────────────────

#include "Plots/ACFWidget/UI/ACFPropertiesWidget.hpp"
#include "Plots/ACFWidget/Core/ACFState.hpp"

struct ACFTraits {
    using State = ACFState;
    using PropertiesWidget = ACFPropertiesWidget;

    static constexpr char const * name = "ACFProperties";

    static std::shared_ptr<State> createState() { return std::make_shared<State>(); }

    static std::unique_ptr<PropertiesWidget> createWidget(
        std::shared_ptr<State> state,
        std::shared_ptr<DataManager> dm)
    {
        return std::make_unique<PropertiesWidget>(std::move(state), std::move(dm));
    }

    static QComboBox * findCombo(PropertiesWidget * w)
    {
        return w->findChild<QComboBox *>("event_key_combo");
    }

    static bool comboContainsKey(PropertiesWidget * w, std::string const & key)
    {
        auto * combo = findCombo(w);
        if (!combo) return false;
        QString target = QString::fromStdString(key);
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemText(i) == target) return true;
        }
        return false;
    }

    static int comboCount(PropertiesWidget * w)
    {
        auto * combo = findCombo(w);
        return combo ? combo->count() : 0;
    }
};

REGISTER_PLOT_WIDGET_COMMON_TESTS(ACFTraits)
