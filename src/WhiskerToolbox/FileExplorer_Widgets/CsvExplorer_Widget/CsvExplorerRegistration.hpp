#ifndef CSV_EXPLORER_WIDGET_REGISTRATION_HPP
#define CSV_EXPLORER_WIDGET_REGISTRATION_HPP

/**
 * @file CsvExplorerRegistration.hpp
 * @brief Registration of CsvExplorer_Widget with DataImportTypeRegistry
 *
 * This module registers the CsvExplorer_Widget as an option in the
 * DataImport_Widget, allowing users to browse CSV/delimited text files
 * before importing data with configurable parsing parameters.
 */

namespace CsvExplorerRegistration {

/**
 * @brief Register CsvExplorer with the DataImportTypeRegistry
 *
 * This function is called during application initialization to make
 * the CSV Explorer available in the Data Import widget.
 */
void registerCsvExplorer();

} // namespace CsvExplorerRegistration

#endif // CSV_EXPLORER_WIDGET_REGISTRATION_HPP
