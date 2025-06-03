#ifndef DATA_MANAGER_WIDGET_UTILS_HPP
#define DATA_MANAGER_WIDGET_UTILS_HPP

#include "DataManager/DataManager.hpp"
#include "IO_Widgets/Media/MediaExport_Widget.hpp"

#include <QComboBox>
#include <QMenu>
#include <QMessageBox>
#include <QString>

#include <functional>
#include <string>
#include <vector>

template <typename T>
inline void populate_move_combo_box(QComboBox * combo_box, DataManager * data_manager, std::string const & active_key) {
    combo_box->clear();
    if (!data_manager) return;
    std::vector<std::string> keys = data_manager->getKeys<T>();
    for (std::string const & key : keys) {
        if (key != active_key) {
            combo_box->addItem(QString::fromStdString(key));
        }
    }
}

/**
 * @brief Create a context menu submenu for move operations
 * 
 * This utility function creates a "Move To" submenu populated with available
 * target keys of the specified template type T, excluding the active key.
 * 
 * @tparam T The data type to filter keys by (e.g., DigitalIntervalSeries, PointData)
 * @param parent_menu The parent menu to add the submenu to
 * @param data_manager Pointer to the DataManager instance
 * @param active_key The currently active key to exclude from options
 * @param move_callback Function to call when a move target is selected, receives target key as string
 * @return QMenu* Pointer to the created submenu (nullptr if no valid targets)
 */
template <typename T>
inline QMenu* create_move_submenu(QMenu* parent_menu, 
                                 DataManager* data_manager, 
                                 std::string const& active_key,
                                 std::function<void(std::string const&)> move_callback) {
    if (!data_manager || !parent_menu) return nullptr;
    
    std::vector<std::string> keys = data_manager->getKeys<T>();
    std::vector<std::string> valid_targets;
    
    // Filter out the active key
    for (std::string const& key : keys) {
        if (key != active_key) {
            valid_targets.push_back(key);
        }
    }
    
    if (valid_targets.empty()) {
        return nullptr; // No valid targets available
    }
    
    QMenu* move_submenu = parent_menu->addMenu("Move To");
    
    for (std::string const& target_key : valid_targets) {
        QAction* move_action = move_submenu->addAction(QString::fromStdString(target_key));
        QObject::connect(move_action, &QAction::triggered, [move_callback, target_key]() {
            move_callback(target_key);
        });
    }
    
    return move_submenu;
}

/**
 * @brief Create a context menu submenu for copy operations
 * 
 * This utility function creates a "Copy To" submenu populated with available
 * target keys of the specified template type T, excluding the active key.
 * 
 * @tparam T The data type to filter keys by (e.g., DigitalIntervalSeries, PointData)
 * @param parent_menu The parent menu to add the submenu to
 * @param data_manager Pointer to the DataManager instance
 * @param active_key The currently active key to exclude from options
 * @param copy_callback Function to call when a copy target is selected, receives target key as string
 * @return QMenu* Pointer to the created submenu (nullptr if no valid targets)
 */
template <typename T>
inline QMenu* create_copy_submenu(QMenu* parent_menu, 
                                 DataManager* data_manager, 
                                 std::string const& active_key,
                                 std::function<void(std::string const&)> copy_callback) {
    if (!data_manager || !parent_menu) return nullptr;
    
    std::vector<std::string> keys = data_manager->getKeys<T>();
    std::vector<std::string> valid_targets;
    
    // Filter out the active key
    for (std::string const& key : keys) {
        if (key != active_key) {
            valid_targets.push_back(key);
        }
    }
    
    if (valid_targets.empty()) {
        return nullptr; // No valid targets available
    }
    
    QMenu* copy_submenu = parent_menu->addMenu("Copy To");
    
    for (std::string const& target_key : valid_targets) {
        QAction* copy_action = copy_submenu->addAction(QString::fromStdString(target_key));
        QObject::connect(copy_action, &QAction::triggered, [copy_callback, target_key]() {
            copy_callback(target_key);
        });
    }
    
    return copy_submenu;
}

/**
 * @brief Add move and copy submenus to an existing context menu
 * 
 * This is a convenience function that adds both move and copy submenus to an existing
 * context menu. If no valid targets exist for either operation, the respective submenu
 * will not be added.
 * 
 * @tparam T The data type to filter keys by
 * @param context_menu The existing context menu to add submenus to
 * @param data_manager Pointer to the DataManager instance
 * @param active_key The currently active key to exclude from options
 * @param move_callback Function to call when a move target is selected
 * @param copy_callback Function to call when a copy target is selected
 * @return std::pair<QMenu*, QMenu*> Pair of pointers to move and copy submenus (nullptr if not created)
 */
template <typename T>
inline std::pair<QMenu*, QMenu*> add_move_copy_submenus(QMenu* context_menu,
                                                        DataManager* data_manager,
                                                        std::string const& active_key,
                                                        std::function<void(std::string const&)> move_callback,
                                                        std::function<void(std::string const&)> copy_callback) {
    QMenu* move_submenu = create_move_submenu<T>(context_menu, data_manager, active_key, move_callback);
    QMenu* copy_submenu = create_copy_submenu<T>(context_menu, data_manager, active_key, copy_callback);
    
    return std::make_pair(move_submenu, copy_submenu);
}

inline bool remove_callback(DataManager * data_manager, std::string const & active_key, int & callback_id) {
    if (!active_key.empty() && callback_id != -1) {
        bool success = data_manager->removeCallbackFromData(active_key, callback_id);
        if (success) {
            callback_id = -1;
            return true;
        } else {
            // Optionally log an error if callback removal failed, though it might not be critical
            // std::cerr << "Line_Widget: Failed to remove callback for key: " << _active_key << std::endl;
            return false;
        }
    }
    return false;
}

template <typename T>
inline bool export_media_frames(DataManager * data_manager,
                                MediaExport_Widget * media_export_options_widget,
                                T save_options_variant,
                                QWidget * parent_ptr,
                                std::vector<size_t> & frame_ids_to_export) {

    auto media_data = data_manager->getData<MediaData>("media");
    if (!media_data) {
        QMessageBox::warning(parent_ptr, "Media Not Found", "Could not find media data to export frames.");
        return false;
    }

    if (frame_ids_to_export.empty()){
        QMessageBox::information(parent_ptr, "No Frames", "No points found in data, so no frames to export.");
        return false;
    }

    if (frame_ids_to_export.size() > 10000) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::warning(parent_ptr, "Large Export",
                                     QString("You are about to export %1 media frames. This might take a while. Are you sure?").arg(frame_ids_to_export.size()),
                                     QMessageBox::Yes|QMessageBox::No);
        if (reply == QMessageBox::No) {
            return false;
        }
    }

    MediaExportOptions media_export_opts = media_export_options_widget->getOptions();

    std::string primary_saved_parent_dir;
    std::visit([&primary_saved_parent_dir](auto& opts) {
        primary_saved_parent_dir = opts.parent_dir;
    }, save_options_variant);

    media_export_opts.image_save_dir = primary_saved_parent_dir;

    int success_count = 0;
    for (size_t frame_id_sz : frame_ids_to_export) {
        int frame_id = static_cast<int>(frame_id_sz);
        save_image(media_data.get(), frame_id, media_export_opts);
        success_count++;
    }
    QMessageBox::information(parent_ptr, "Media Export Complete", QString("Successfully exported %1 of %2 frames.").arg(success_count).arg(frame_ids_to_export.size()));
    return true;
}

#endif // DATA_MANGER_WIDGET_UTILS_HPP
