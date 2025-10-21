#ifndef ABSL_LOG_INSTANTIATIONS_H
#define ABSL_LOG_INSTANTIATIONS_H

// Explicit template instantiations for Abseil logging to fix Clang 18 linking issues
// See: https://github.com/llvm/llvm-project/issues/102443#issuecomment-2275698190

#include "absl/log/internal/log_message.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace log_internal {

// Explicit instantiations for all types used in OR-Tools
template LogMessage& LogMessage::operator<<(const bool&);
template LogMessage& LogMessage::operator<<(const char&);
template LogMessage& LogMessage::operator<<(const signed char&);
template LogMessage& LogMessage::operator<<(const unsigned char&);
template LogMessage& LogMessage::operator<<(const short&);
template LogMessage& LogMessage::operator<<(const int&);
template LogMessage& LogMessage::operator<<(const long&);
template LogMessage& LogMessage::operator<<(const long long&);
template LogMessage& LogMessage::operator<<(const unsigned int&);
template LogMessage& LogMessage::operator<<(const unsigned long&);
template LogMessage& LogMessage::operator<<(const size_t&);
template LogMessage& LogMessage::operator<<(const double&);
template LogMessage& LogMessage::operator<<(const float&);

} // namespace log_internal
ABSL_NAMESPACE_END
} // namespace absl

#endif // ABSL_LOG_INSTANTIATIONS_H
