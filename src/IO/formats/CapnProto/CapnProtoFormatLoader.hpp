#ifndef CAPNPROTO_FORMAT_LOADER_HPP
#define CAPNPROTO_FORMAT_LOADER_HPP

#include "datamanagerio_capnproto_export.h"

#include "../../core/IFormatLoader.hpp"

/**
 * @brief CapnProto format loader
 * 
 * This loader provides CapnProto/binary loading capability for LineData and other data types.
 * It wraps the existing CapnProto loading functionality.
 */
class DATAMANAGERIO_CAPNPROTO_EXPORT CapnProtoFormatLoader : public IFormatLoader {
public:
    CapnProtoFormatLoader() = default;
    ~CapnProtoFormatLoader() override = default;

    /**
     * @brief Load data from CapnProto file
     */
    LoadResult load(std::string const & filepath,
                    DM_DataType dataType,
                    nlohmann::json const & config) const override;

    /**
     * @brief Save data to CapnProto file
     */
    LoadResult save(std::string const & filepath,
                    DM_DataType dataType,
                    nlohmann::json const & config,
                    void const * data) const override;

    /**
     * @brief Check if this loader supports the format/dataType combination
     */
    bool supportsFormat(std::string const & format, DM_DataType dataType) const override;

    /**
     * @brief Return metadata for all save operations this loader supports
     */
    std::vector<SaverInfo> getSaverInfo() const override;

    /**
     * @brief Get loader name for logging
     */
    std::string getLoaderName() const override;

private:
    /**
     * @brief Load LineData from CapnProto file using existing functionality
     */
    static LoadResult loadLineDataCapnProto(std::string const & filepath,
                                            nlohmann::json const & config);
};

#endif// CAPNPROTO_FORMAT_LOADER_HPP
