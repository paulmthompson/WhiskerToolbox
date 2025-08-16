#ifndef DATAMANAGER_IO_CAPNPROTO_CAPNPROTOLOADER_HPP
#define DATAMANAGER_IO_CAPNPROTO_CAPNPROTOLOADER_HPP

#include "../DataLoader.hpp"
#include "../DataFactory.hpp"
#include <set>
#include <capnp/common.h>
#include <kj/array.h>

// Forward declarations
namespace capnp { 
    struct ReaderOptions; 
}

/**
 * @brief CapnProto format data loader
 * 
 * This loader handles loading data stored in CapnProto format.
 * Currently supports LineData, with extensibility for other data types.
 */
class CapnProtoLoader : public DataLoader {
public:
    CapnProtoLoader();
    
    std::string getFormatId() const override;
    bool supportsDataType(DM_DataType data_type) const override;
    LoadResult loadData(
        std::string const& file_path,
        DM_DataType data_type,
        nlohmann::json const& config,
        DataFactory* factory
    ) const override;

private:
    std::set<DM_DataType> _supported_types;
    
    // Type-specific loading methods
    LoadResult loadLineData(std::string const& file_path, nlohmann::json const& config, DataFactory* factory) const;
    // Future: LoadResult loadPointData(std::string const& file_path, nlohmann::json const& config, DataFactory* factory) const;
    
    // Raw data extraction (no dependency on concrete data types)
    LineDataRaw extractLineDataRaw(kj::ArrayPtr<capnp::word const> messageData, capnp::ReaderOptions const& options) const;
};

#endif // DATAMANAGER_IO_CAPNPROTO_CAPNPROTOLOADER_HPP
