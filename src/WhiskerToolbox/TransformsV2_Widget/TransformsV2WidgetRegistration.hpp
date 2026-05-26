#ifndef TRANSFORMS_V2_WIDGET_REGISTRATION_HPP
#define TRANSFORMS_V2_WIDGET_REGISTRATION_HPP

#include <QString>

#include <memory>

class EditorRegistry;
class DataManager;

namespace TransformsV2WidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager,
                   QString const & pipeline_config_dir);

} // namespace TransformsV2WidgetModule

#endif // TRANSFORMS_V2_WIDGET_REGISTRATION_HPP
