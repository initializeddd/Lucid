#pragma once
#include "modules/AppleIpsReport.hpp"
#include "modules/CrashSymbols.hpp"
namespace lucid::modules {
// Resolves an already parsed IPS image-relative frame against an explicit local
// TSV source map. A result is only a candidate: neither IPS names nor mappings
// authenticate binaries or source. Caller must check project containment,
// unsaved documents and current source contents before offering navigation.
// Duplicate image names and overlapping matching ranges are ambiguous.
[[nodiscard]] CrashLocation locateIpsSource(const AppleIpsReport& report, const IpsFrame& frame, const CrashSourceMap& map);
}
