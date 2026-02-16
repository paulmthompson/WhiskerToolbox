#ifndef BINARY_EXPLORER_WIDGET_REGISTRATION_HPP
#define BINARY_EXPLORER_WIDGET_REGISTRATION_HPP

/**
 * @file BinaryExplorerRegistration.hpp
 * @brief Registration of BinaryExplorer_Widget with DataImportTypeRegistry
 * 
 * This module registers the BinaryExplorer_Widget as an option in the
 * DataImport_Widget, allowing users to browse raw binary files before
 * importing data with configurable parsing parameters.
 */

namespace BinaryExplorerRegistration {

/**
 * @brief Register BinaryExplorer with the DataImportTypeRegistry
 * 
 * This function is called during application initialization to make
 * the Binary Explorer available in the Data Import widget.
 */
void registerBinaryExplorer();

} // namespace BinaryExplorerRegistration

#endif // BINARY_EXPLORER_WIDGET_REGISTRATION_HPP
