#include "CsvExplorerRegistration.hpp"
#include "CsvExplorer_Widget.hpp"
#include "DataImport_Widget/DataImportTypeRegistry.hpp"

#include <iostream>

namespace {

struct CsvExplorerRegistrar {
    CsvExplorerRegistrar() {
        CsvExplorerRegistration::registerCsvExplorer();
    }
};

static CsvExplorerRegistrar const csv_explorer_registrar;

} // anonymous namespace

namespace CsvExplorerRegistration {

void registerCsvExplorer() {
    std::cout << "CsvExplorerRegistration: Registering CSV Explorer widget" << std::endl;

    DataImportTypeRegistry::instance().registerType(
        "CsvExplorer",
        ImportWidgetFactory{
            .display_name = "CSV File Explorer (.csv, .tsv, .txt)",
            .create_widget = [](std::shared_ptr<DataManager> dm, QWidget * parent) -> QWidget * {
                return new CsvExplorer_Widget(std::move(dm), parent);
            }
        }
    );

    std::cout << "CsvExplorerRegistration: CSV Explorer widget registered successfully" << std::endl;
}

} // namespace CsvExplorerRegistration
