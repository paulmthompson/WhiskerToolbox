/**
 * @file CommandUIHints.hpp
 * @brief ParameterUIHints specializations for all command parameter structs
 */

#ifndef COMMAND_UI_HINTS_HPP
#define COMMAND_UI_HINTS_HPP

#include "AddInterval.hpp"
#include "CopyByTimeRange.hpp"
#include "ForEachKey.hpp"
#include "IO/LoadData.hpp"
#include "MoveByTimeRange.hpp"
#include "IO/SaveData.hpp"

#include "ParameterSchema/ParameterSchema.hpp"


template<>
struct ParameterUIHints<commands::MoveByTimeRangeParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("source_key")) {
            f->tooltip = "Data key of the source object to move entities from";
        }
        if (auto * f = schema.field("destination_key")) {
            f->tooltip = "Data key of the destination object to move entities to";
        }
        if (auto * f = schema.field("start_frame")) {
            f->tooltip = "Start of the time range (inclusive)";
        }
        if (auto * f = schema.field("end_frame")) {
            f->tooltip = "End of the time range (inclusive)";
        }
    }
};

template<>
struct ParameterUIHints<commands::CopyByTimeRangeParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("source_key")) {
            f->tooltip = "Data key of the source object to copy entities from";
        }
        if (auto * f = schema.field("destination_key")) {
            f->tooltip = "Data key of the destination object to copy entities to";
        }
        if (auto * f = schema.field("start_frame")) {
            f->tooltip = "Start of the time range (inclusive)";
        }
        if (auto * f = schema.field("end_frame")) {
            f->tooltip = "End of the time range (inclusive)";
        }
    }
};

template<>
struct ParameterUIHints<commands::AddIntervalParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("interval_key")) {
            f->tooltip = "Data key of the DigitalIntervalSeries to append to";
        }
        if (auto * f = schema.field("start_frame")) {
            f->tooltip = "Start frame of the interval (inclusive)";
        }
        if (auto * f = schema.field("end_frame")) {
            f->tooltip = "End frame of the interval (inclusive)";
        }
        if (auto * f = schema.field("create_if_missing")) {
            f->tooltip = "Create the DigitalIntervalSeries if it does not exist";
        }
    }
};

template<>
struct ParameterUIHints<commands::ForEachKeyParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("items")) {
            f->tooltip = "List of values to iterate over";
        }
        if (auto * f = schema.field("variable")) {
            f->tooltip = "Template variable name bound to each item (e.g. \"key\")";
        }
        if (auto * f = schema.field("commands")) {
            f->tooltip = "Sub-commands executed once per item with ${variable} substitution";
            f->is_advanced = true;
        }
    }
};

template<>
struct ParameterUIHints<commands::SaveDataParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("data_key")) {
            f->tooltip = "Key of the data object in DataManager to save";
        }
        if (auto * f = schema.field("format")) {
            f->tooltip = "Output format (e.g. \"csv\", \"capnproto\", \"opencv\")";
        }
        if (auto * f = schema.field("path")) {
            f->tooltip = "Output file or directory path";
        }
        if (auto * f = schema.field("format_options")) {
            f->tooltip = "Format-specific options passed to the saver (optional)";
            f->is_advanced = true;
        }
    }
};

template<>
struct ParameterUIHints<commands::LoadDataParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("data_key")) {
            f->tooltip = "Key to store the loaded data under in DataManager";
        }
        if (auto * f = schema.field("data_type")) {
            f->tooltip = "Data type to load (e.g. \"PointData\", \"LineData\", \"MaskData\")";
        }
        if (auto * f = schema.field("filepath")) {
            f->tooltip = "Path to the input file to load";
        }
        if (auto * f = schema.field("format")) {
            f->tooltip = "Input format (e.g. \"csv\", \"capnproto\")";
        }
        if (auto * f = schema.field("format_options")) {
            f->tooltip = "Format-specific loader options (optional)";
            f->is_advanced = true;
        }
    }
};

#endif// COMMAND_UI_HINTS_HPP
