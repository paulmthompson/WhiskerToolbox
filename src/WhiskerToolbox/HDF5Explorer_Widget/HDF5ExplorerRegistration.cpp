#include "HDF5ExplorerRegistration.hpp"
#include "HDF5Explorer_Widget.hpp"
#include "DataImport_Widget/DataImportTypeRegistry.hpp"

#include <iostream>

namespace {

/**
 * @brief Static registrar that registers HDF5Explorer at program startup
 */
struct HDF5ExplorerRegistrar {
    HDF5ExplorerRegistrar() {
        HDF5ExplorerRegistration::registerHDF5Explorer();
    }
};

// Static instance ensures registration happens at program startup
static HDF5ExplorerRegistrar const hdf5_explorer_registrar;

} // anonymous namespace

namespace HDF5ExplorerRegistration {

void registerHDF5Explorer() {
    std::cout << "HDF5ExplorerRegistration: Registering HDF5 Explorer widget" << std::endl;
    
    DataImportTypeRegistry::instance().registerType(
        "HDF5Explorer",
        ImportWidgetFactory{
            .display_name = "HDF5 File Explorer",
            .create_widget = [](std::shared_ptr<DataManager> dm, QWidget * parent) -> QWidget * {
                return new HDF5Explorer_Widget(std::move(dm), parent);
            }
        }
    );
    
    std::cout << "HDF5ExplorerRegistration: HDF5 Explorer widget registered successfully" << std::endl;
}

} // namespace HDF5ExplorerRegistration
