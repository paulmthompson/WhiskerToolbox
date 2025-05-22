#ifndef DATA_MANAGER_WIDGET_UTILS_HPP
#define DATA_MANAGER_WIDGET_UTILS_HPP

#include "DataManager/DataManager.hpp"
#include "IO_Widgets/Media/MediaExport_Widget.hpp"

#include <QComboBox>
#include <QMessageBox>
#include <QString>

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
