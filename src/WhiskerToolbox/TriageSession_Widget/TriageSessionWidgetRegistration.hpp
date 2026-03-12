/**
 * @file TriageSessionWidgetRegistration.hpp
 * @brief Module registration for the TriageSession widget
 */

#ifndef TRIAGE_SESSION_WIDGET_REGISTRATION_HPP
#define TRIAGE_SESSION_WIDGET_REGISTRATION_HPP

#include <memory>

class EditorRegistry;
class DataManager;

namespace commands {
class CommandRecorder;
}// namespace commands

namespace TriageSessionWidgetModule {

void registerTypes(EditorRegistry * registry,
                   const std::shared_ptr<DataManager>& data_manager,
                   commands::CommandRecorder * recorder = nullptr);

}// namespace TriageSessionWidgetModule

#endif// TRIAGE_SESSION_WIDGET_REGISTRATION_HPP
