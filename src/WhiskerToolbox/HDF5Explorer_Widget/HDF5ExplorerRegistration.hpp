#ifndef HDF5_EXPLORER_WIDGET_REGISTRATION_HPP
#define HDF5_EXPLORER_WIDGET_REGISTRATION_HPP

/**
 * @file HDF5ExplorerRegistration.hpp
 * @brief Registration of HDF5Explorer_Widget with DataImportTypeRegistry
 * 
 * This module registers the HDF5Explorer_Widget as an option in the
 * DataImport_Widget, allowing users to browse HDF5 files before importing
 * specific datasets.
 * 
 * @note Only available when ENABLE_HDF5 is defined.
 */

namespace HDF5ExplorerRegistration {

/**
 * @brief Register HDF5Explorer with the DataImportTypeRegistry
 * 
 * This function is called during application initialization to make
 * the HDF5 Explorer available in the Data Import widget.
 */
void registerHDF5Explorer();

} // namespace HDF5ExplorerRegistration

#endif // HDF5_EXPLORER_WIDGET_REGISTRATION_HPP
