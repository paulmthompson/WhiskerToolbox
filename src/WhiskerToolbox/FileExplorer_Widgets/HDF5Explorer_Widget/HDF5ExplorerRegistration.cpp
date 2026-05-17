#include "HDF5ExplorerRegistration.hpp"

#include "DataImport_Widget/DataImportTypeRegistry.hpp"
#include "HDF5Explorer_Widget.hpp"

#include <spdlog/spdlog.h>

namespace HDF5ExplorerRegistration {

void registerHDF5Explorer() {
    spdlog::debug("HDF5ExplorerRegistration: registering HDF5 Explorer with DataImportTypeRegistry");

    DataImportTypeRegistry::instance().registerType(
            "HDF5Explorer",
            ImportWidgetFactory{
                    .display_name = "HDF5 File Explorer",
                    .create_widget = [](std::shared_ptr<DataManager> dm, QWidget * parent) -> QWidget * {
                        return new HDF5Explorer_Widget(std::move(dm), parent);
                    }});

    spdlog::debug("HDF5ExplorerRegistration: HDF5 Explorer registration complete");
}

}// namespace HDF5ExplorerRegistration
