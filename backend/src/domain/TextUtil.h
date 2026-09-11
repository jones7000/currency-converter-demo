#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace currency::domain {

// Currency codes arriving over the wire may be lower/mixed case; frankfurter.dev
// and our own currency list both use uppercase ISO 4217 codes, so requests are
// normalized once, here, rather than duplicating case-folding logic at every
// comparison site.
inline std::string toUpper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                    [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

} // namespace currency::domain
