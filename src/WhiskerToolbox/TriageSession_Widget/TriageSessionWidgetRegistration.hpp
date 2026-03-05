/**
 * @file TriageSessionWidgetRegistration.hpp
 * @brief Module registration for the TriageSession widget
 */

#ifndef TRIAGE_SESSION_WIDGET_REGISTRATION_HPP
#define TRIAGE_SESSION_WIDGET_REGISTRATION_HPP

#include <memory>

class EditorRegistry;
class DataManager;

namespace TriageSessionWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}// namespace TriageSessionWidgetModule

#endif// TRIAGE_SESSION_WIDGET_REGISTRATION_HPP
