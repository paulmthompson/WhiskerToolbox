#ifndef WHISKERTOOLBOX_LINE_DATA_CAPNP_HPP
#define WHISKERTOOLBOX_LINE_DATA_CAPNP_HPP

#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/std/iostream.h>

#include <memory>

class LineData;

kj::Array<capnp::word> serializeLineData(LineData const * lineData);

std::shared_ptr<LineData> deserializeLineData(
    kj::ArrayPtr<capnp::word const> messageData,
    capnp::ReaderOptions const& options 
);

#endif//WHISKERTOOLBOX_LINE_DATA_CAPNP_HPP
