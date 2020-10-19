#include "common/http/path_utility.h"

#include "common/chromium_url/url_canon.h"
#include "common/chromium_url/url_canon_stdstring.h"
#include "common/common/logger.h"

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/types/optional.h"

namespace Envoy {
namespace Http {

namespace {
absl::optional<std::string> canonicalizePath(absl::string_view original_path) {
  std::string canonical_path;
  chromium_url::Component in_component(0, original_path.size());
  chromium_url::Component out_component;
  chromium_url::StdStringCanonOutput output(&canonical_path);
  if (!chromium_url::CanonicalizePath(original_path.data(), in_component, &output,
                                      &out_component)) {
    return absl::nullopt;
  } else {
    output.Complete();
    return absl::make_optional(std::move(canonical_path));
  }
}
} // namespace

/* static */
bool PathUtil::canonicalPath(RequestHeaderMap& headers) {
  ASSERT(headers.Path());
  const auto original_path = headers.getPathValue();
  // canonicalPath is supposed to apply on path component in URL instead of :path header
  const auto query_pos = original_path.find('?');
  auto normalized_path_opt = canonicalizePath(
      query_pos == original_path.npos
          ? original_path
          : absl::string_view(original_path.data(), query_pos) // '?' is not included
  );

  if (!normalized_path_opt.has_value()) {
    return false;
  }
  auto& normalized_path = normalized_path_opt.value();
  const absl::string_view query_suffix =
      query_pos == original_path.npos
          ? absl::string_view{}
          : absl::string_view{original_path.data() + query_pos, original_path.size() - query_pos};
  if (!query_suffix.empty()) {
    normalized_path.insert(normalized_path.end(), query_suffix.begin(), query_suffix.end());
  }
  headers.setPath(normalized_path);
  return true;
}

void PathUtil::mergeSlashes(RequestHeaderMap& headers) {
  ASSERT(headers.Path());
  const auto original_path = headers.getPathValue();
  // Only operate on path component in URL.
  const absl::string_view::size_type query_start = original_path.find('?');
  const absl::string_view path = original_path.substr(0, query_start);
  const absl::string_view query = absl::ClippedSubstr(original_path, query_start);
  if (path.find("//") == absl::string_view::npos) {
    return;
  }
  const absl::string_view path_prefix = absl::StartsWith(path, "/") ? "/" : absl::string_view();
  const absl::string_view path_suffix = absl::EndsWith(path, "/") ? "/" : absl::string_view();
  headers.setPath(absl::StrCat(path_prefix,
                               absl::StrJoin(absl::StrSplit(path, '/', absl::SkipEmpty()), "/"),
                               path_suffix, query));
}

absl::string_view PathUtil::removeQueryAndFragment(const absl::string_view path) {
  absl::string_view ret = path;
  // Trim query parameters and/or fragment if present.
  size_t offset = ret.find_first_of("?#");
  if (offset != absl::string_view::npos) {
    ret.remove_suffix(ret.length() - offset);
  }
  return ret;
}

std::string PathUtil::decodeAsciiPrintableChars(const absl::string_view path) {
  const auto try_parse_hex_char = [](const char ch) -> int {
    if (('0' <= ch) && (ch <= '9')) {
      return ch - '0';
    } else if (('A' <= ch) && (ch <= 'F')) {
      return 10 + (ch - 'A');
    } else if (('a' <= ch) && (ch <= 'f')) {
      return 10 + (ch - 'a');
    }
    return -1;
  };

  const auto try_parse_encoded_byteval = [&](std::size_t pos) -> char {
    const char fst = path[pos + 1];
    const char snd = path[pos + 2];

    int fst_char = try_parse_hex_char(fst);
    int snd_char = try_parse_hex_char(snd);

    if ((fst_char != -1) && (snd_char != -1)) {
      return (fst_char << 4) | snd_char;
    }

    return 0;
  };

  std::string result(path.size(), 0);
  std::size_t cursor = 0;

  for (std::size_t i = 0; i < path.length(); ++i) {
    if (path[i] == '%' && i + 2 < path.length()) {
      int ch = try_parse_encoded_byteval(i);
      if ((0x1f < ch) && (ch < 0x7f) && (ch != '/')) {
        result[cursor++] = ch;
        i += 2;
      } else {
        result[cursor++] = path[i];
      }
    } else {
      result[cursor++] = path[i];
    }
  }

  result.resize(cursor);
  return result;
}

} // namespace Http
} // namespace Envoy
