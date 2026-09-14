#ifndef MEDIA_TOOL_ID_HPP
#define MEDIA_TOOL_ID_HPP

/**
 * @file MediaToolId.hpp
 * @brief Identifiers for Media Viewer toolbar tools
 */

#include <QMetaType>

/**
 * @brief Active tool modes for the Media Viewer left toolbar
 */
enum class MediaToolId {
    Select///< Pointer / selection tool (default)
};

Q_DECLARE_METATYPE(MediaToolId)

#endif// MEDIA_TOOL_ID_HPP
