/**
 * @file TriageSessionProperties_Widget.hpp
 * @brief Properties widget for the Triage Session
 *
 * Empty placeholder widget. Will be populated in Step 2.3 with
 * Mark/Commit/Recall buttons, pipeline display, and tracked-regions summary.
 */

#ifndef TRIAGE_SESSION_PROPERTIES_WIDGET_HPP
#define TRIAGE_SESSION_PROPERTIES_WIDGET_HPP

#include <QWidget>

#include <memory>

class TriageSessionState;

/**
 * @brief Properties panel for the Triage Session widget
 *
 * This is a properties-only widget (no separate view panel).
 * Currently an empty shell — UI controls will be added in Step 2.3.
 */
class TriageSessionProperties_Widget : public QWidget {
    Q_OBJECT

public:
    explicit TriageSessionProperties_Widget(std::shared_ptr<TriageSessionState> state,
                                            QWidget * parent = nullptr);
    ~TriageSessionProperties_Widget() override = default;

private:
    std::shared_ptr<TriageSessionState> _state;
};

#endif// TRIAGE_SESSION_PROPERTIES_WIDGET_HPP
