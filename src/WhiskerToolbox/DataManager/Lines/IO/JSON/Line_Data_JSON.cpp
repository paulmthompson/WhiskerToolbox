
#include "Line_Data_JSON.hpp"

#include "Lines/IO/Binary/Line_Data_Binary.hpp"
#include "Lines/IO/CSV/Line_Data_CSV.hpp"
#include "Lines/Line_Data.hpp"
#include "loaders/loading_utils.hpp"
#include "utils/json_helpers.hpp"


std::shared_ptr<LineData> load_into_LineData(std::string const & file_path, nlohmann::basic_json<> const & item) {

    auto line_data = std::make_shared<LineData>();

    if (!requiredFieldsExist(item,
                             {"format"},
                             "Error: Missing required field format. Supported options include binary, csv, hdf5"))
    {
        return std::make_shared<LineData>();
    }

    auto const format = item["format"];

    if (format == "csv") {

        /*

        if (!requiredFieldsExist(item,
                                 {"format"},
                                 "Error: Missing required csv fields for LineData"))
        {
            return std::make_shared<LineData>();
        }

        */

    } else if (format == "binary") {

        auto opts = BinaryLineLoaderOptions();
        opts.file_path = file_path;

        line_data = load(opts);

    } else if (format == "hdf5") {

    } else {

    }

    change_image_size_json(line_data, item);

    return line_data;
}
