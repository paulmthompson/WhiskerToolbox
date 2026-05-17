#include "NpyExplorerRegistration.hpp"

#include "DataImport_Widget/DataImportTypeRegistry.hpp"
#include "NpyExplorer_Widget.hpp"

#include <spdlog/spdlog.h>

namespace NpyExplorerRegistration {

void registerNpyExplorer() {
    spdlog::debug("NpyExplorerRegistration: registering NumPy Explorer with DataImportTypeRegistry");

    DataImportTypeRegistry::instance().registerType(
            "NpyExplorer",
            ImportWidgetFactory{
                    .display_name = "NumPy File Explorer (.npy)",
                    .create_widget = [](std::shared_ptr<DataManager> dm, QWidget * parent) -> QWidget * {
                        return new NpyExplorer_Widget(std::move(dm), parent);
                    }});

    spdlog::debug("NpyExplorerRegistration: NumPy Explorer registration complete");
}

}// namespace NpyExplorerRegistration
