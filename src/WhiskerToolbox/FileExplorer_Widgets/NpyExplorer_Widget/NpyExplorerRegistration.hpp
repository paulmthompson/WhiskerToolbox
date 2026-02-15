#ifndef NPY_EXPLORER_WIDGET_REGISTRATION_HPP
#define NPY_EXPLORER_WIDGET_REGISTRATION_HPP

/**
 * @file NpyExplorerRegistration.hpp
 * @brief Registration of NpyExplorer_Widget with DataImportTypeRegistry
 * 
 * This module registers the NpyExplorer_Widget as an option in the
 * DataImport_Widget, allowing users to browse NumPy .npy files before
 * importing specific arrays.
 */

namespace NpyExplorerRegistration {

/**
 * @brief Register NpyExplorer with the DataImportTypeRegistry
 * 
 * This function is called during application initialization to make
 * the NumPy Explorer available in the Data Import widget.
 */
void registerNpyExplorer();

} // namespace NpyExplorerRegistration

#endif // NPY_EXPLORER_WIDGET_REGISTRATION_HPP
