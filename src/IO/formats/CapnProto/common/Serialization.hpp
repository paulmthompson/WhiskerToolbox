#ifndef WHISKERTOOLBOX_IO_CAPNPROTO_SERIALIZATION_HPP
#define WHISKERTOOLBOX_IO_CAPNPROTO_SERIALIZATION_HPP

#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/std/iostream.h>
#include <memory>

// Forward declarations to avoid including concrete types
class LineData;

namespace IO::CapnProto {

/**
 * @brief Serializes LineData to CapnProto binary format
 * @param lineData The LineData object to serialize
 * @return Array of capnp words representing the serialized data
 */
kj::Array<capnp::word> serializeLineData(LineData const* lineData);

/**
 * @brief Deserializes CapnProto binary data to LineData
 * @param messageData Array of capnp words containing the serialized data
 * @param options CapnProto reader options for deserialization limits
 * @return Shared pointer to the deserialized LineData object
 */
std::shared_ptr<LineData> deserializeLineData(
    kj::ArrayPtr<capnp::word const> messageData,
    capnp::ReaderOptions const& options = capnp::ReaderOptions{}
);

} // namespace IO::CapnProto

#endif // WHISKERTOOLBOX_IO_CAPNPROTO_SERIALIZATION_HPP
