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
 * Called from application startup (for example `Neuralyzer` `main`) after
 * optional `spdlog` level configuration. Do not rely on static initialization to
 * register this type: `main` already references this function so the static
 * library object is linked, and a second static registrar would double-register.
 */
void registerHDF5Explorer();

}// namespace HDF5ExplorerRegistration

#endif// HDF5_EXPLORER_WIDGET_REGISTRATION_HPP
