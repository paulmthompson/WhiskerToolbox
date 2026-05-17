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
 * Called from application startup (for example `WhiskerToolbox` `main`) after
 * optional `spdlog` level configuration. Do not rely on static initialization to
 * register this type: `main` already references this function so the static
 * library object is linked, and a second static registrar would double-register.
 */
void registerNpyExplorer();

}// namespace NpyExplorerRegistration

#endif// NPY_EXPLORER_WIDGET_REGISTRATION_HPP
