#include "BinaryExplorerRegistration.hpp"
#include "BinaryExplorer_Widget.hpp"
#include "DataImport_Widget/DataImportTypeRegistry.hpp"

#include <iostream>

namespace {

struct BinaryExplorerRegistrar {
    BinaryExplorerRegistrar() {
        BinaryExplorerRegistration::registerBinaryExplorer();
    }
};

static BinaryExplorerRegistrar const binary_explorer_registrar;

} // anonymous namespace

namespace BinaryExplorerRegistration {

void registerBinaryExplorer() {
    std::cout << "BinaryExplorerRegistration: Registering Binary Explorer widget" << std::endl;
    
    DataImportTypeRegistry::instance().registerType(
        "BinaryExplorer",
        ImportWidgetFactory{
            .display_name = "Binary File Explorer (.bin, .dat, .raw)",
            .create_widget = [](std::shared_ptr<DataManager> dm, QWidget * parent) -> QWidget * {
                return new BinaryExplorer_Widget(std::move(dm), parent);
            }
        }
    );
    
    std::cout << "BinaryExplorerRegistration: Binary Explorer widget registered successfully" << std::endl;
}

} // namespace BinaryExplorerRegistration
