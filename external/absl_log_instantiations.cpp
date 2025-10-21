// Explicit template instantiations for Abseil logging to fix Clang 18 linking issues
// This file must be compiled and linked with OR-Tools

#include "absl/log/internal/log_message.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace log_internal {

// Explicit instantiations for boolean and character types
template LogMessage& LogMessage::operator<<(const bool&);
template LogMessage& LogMessage::operator<<(const char&);
template LogMessage& LogMessage::operator<<(const signed char&);
template LogMessage& LogMessage::operator<<(const unsigned char&);

// Explicit instantiations for integer types
template LogMessage& LogMessage::operator<<(const short&);
template LogMessage& LogMessage::operator<<(const int&);
template LogMessage& LogMessage::operator<<(const long&);
template LogMessage& LogMessage::operator<<(const long long&);
template LogMessage& LogMessage::operator<<(const unsigned short&);
template LogMessage& LogMessage::operator<<(const unsigned int&);
template LogMessage& LogMessage::operator<<(const unsigned long&);
template LogMessage& LogMessage::operator<<(const unsigned long long&);

// Explicit instantiations for floating point types
template LogMessage& LogMessage::operator<<(const double&);
template LogMessage& LogMessage::operator<<(const float&);

// size_t is often different from the above depending on platform
#if SIZE_MAX == UINT64_MAX && !defined(__LP64__)
// 64-bit size_t on 32-bit platform (rare)
template LogMessage& LogMessage::operator<<(const size_t&);
#elif SIZE_MAX == UINT32_MAX
// 32-bit size_t
template LogMessage& LogMessage::operator<<(const size_t&);
#endif

} // namespace log_internal
ABSL_NAMESPACE_END
} // namespace absl
