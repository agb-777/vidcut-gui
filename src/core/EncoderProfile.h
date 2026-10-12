#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/EncodeSettings.h"
#include "core/MediaInfo.h"

namespace vidcut {

// Encoder arguments that match the source stream (codec, profile, level, pixel
// format, colour info) so re-encoded edges can be concatenated with the
// stream-copied middle. Returns nullopt if the codec is not supported by smartcut.
std::optional<std::vector<std::string>> buildEncoderArgs(const VideoInfo& v,
                                                         const EncodeSettings& s);

// Full re-encode with libx264 for codecs smartcut cannot handle.
std::vector<std::string> fallbackEncoderArgs(const VideoInfo& v,
                                             const EncodeSettings& s);

}  // namespace vidcut
