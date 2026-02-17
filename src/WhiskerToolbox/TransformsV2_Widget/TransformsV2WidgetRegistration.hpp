#ifndef TRANSFORMS_V2_WIDGET_REGISTRATION_HPP
#define TRANSFORMS_V2_WIDGET_REGISTRATION_HPP

#include <memory>

class EditorRegistry;
class DataManager;

namespace TransformsV2WidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

} // namespace TransformsV2WidgetModule

#endif // TRANSFORMS_V2_WIDGET_REGISTRATION_HPP
