#ifndef DATAMANAGER_IO_CAPNPROTO_CAPNPROTOLOADER_HPP
#define DATAMANAGER_IO_CAPNPROTO_CAPNPROTOLOADER_HPP

#include "IO/interface/DataFactory.hpp"
#include "IO/interface/DataLoader.hpp"
#include "IO/interface/IOTypes.hpp"
#include <capnp/common.h>
#include <kj/array.h>
#include <set>

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
    bool supportsDataType(IODataType data_type) const override;
    LoadResult loadData(
        std::string const& file_path,
        IODataType data_type,
        nlohmann::json const& config,
        DataFactory* factory
    ) const override;

private:
    std::set<IODataType> _supported_types;

    // Type-specific loading methods
    LoadResult loadLineData(std::string const& file_path, nlohmann::json const& config, DataFactory* factory) const;
    // Future: LoadResult loadPointData(std::string const& file_path, nlohmann::json const& config, DataFactory* factory) const;
    
    // Raw data extraction (no dependency on concrete data types)
    LineDataRaw extractLineDataRaw(kj::ArrayPtr<capnp::word const> messageData, capnp::ReaderOptions const& options) const;
};

#endif // DATAMANAGER_IO_CAPNPROTO_CAPNPROTOLOADER_HPP
