#include "NpyExplorerRegistration.hpp"
#include "NpyExplorer_Widget.hpp"
#include "DataImport_Widget/DataImportTypeRegistry.hpp"

#include <iostream>

namespace {

/**
 * @brief Static registrar that registers NpyExplorer at program startup
 */
struct NpyExplorerRegistrar {
    NpyExplorerRegistrar() {
        NpyExplorerRegistration::registerNpyExplorer();
    }
};

// Static instance ensures registration happens at program startup
static NpyExplorerRegistrar const npy_explorer_registrar;

} // anonymous namespace

namespace NpyExplorerRegistration {

void registerNpyExplorer() {
    std::cout << "NpyExplorerRegistration: Registering NumPy Explorer widget" << std::endl;
    
    DataImportTypeRegistry::instance().registerType(
        "NpyExplorer",
        ImportWidgetFactory{
            .display_name = "NumPy File Explorer (.npy)",
            .create_widget = [](std::shared_ptr<DataManager> dm, QWidget * parent) -> QWidget * {
                return new NpyExplorer_Widget(std::move(dm), parent);
            }
        }
    );
    
    std::cout << "NpyExplorerRegistration: NumPy Explorer widget registered successfully" << std::endl;
}

} // namespace NpyExplorerRegistration
