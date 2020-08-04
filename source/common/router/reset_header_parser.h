#pragma once

#include <chrono>
#include <memory>
#include <vector>

#include "envoy/common/time.h"
#include "envoy/config/route/v3/route_components.pb.h"
#include "envoy/http/header_map.h"

#include "common/protobuf/protobuf.h"

#include "absl/types/optional.h"

namespace Envoy {
namespace Router {

class ResetHeaderParser;
using ResetHeaderParserSharedPtr = std::shared_ptr<ResetHeaderParser>;

enum class ResetHeaderFormat { Seconds, UnixTimestamp };

/**
 * ResetHeaderParser specifies a header name and a format to match against
 * response headers that are used to signal a rate limit interval reset, such
 * as Retry-After or X-RateLimit-Reset.
 */
class ResetHeaderParser {
public:
  /**
   * Build a vector of ResetHeaderParserSharedPtr given input config.
   */
  static std::vector<ResetHeaderParserSharedPtr> buildResetHeaderParserVector(
      const Protobuf::RepeatedPtrField<envoy::config::route::v3::RetryPolicy::ResetHeader>&
          reset_headers) {
    std::vector<ResetHeaderParserSharedPtr> ret;
    for (const auto& reset_header : reset_headers) {
      ret.emplace_back(std::make_shared<ResetHeaderParser>(reset_header));
    }
    return ret;
  }

  ResetHeaderParser(const envoy::config::route::v3::RetryPolicy::ResetHeader& config);

  /**
   * Iterate over the headers, choose the first one that matches by name, and try to parse its
   * value.
   */
  absl::optional<std::chrono::milliseconds>
  parseInterval(TimeSource& time_source, const Http::HeaderMap& headers) const;

private:
  const Http::LowerCaseString name_;
  ResetHeaderFormat format_;
};

} // namespace Router
} // namespace Envoy
