#include "library/audio_tags.h"

#include "core/paths.h"
#include "fileref.h"
#include "tag.h"

namespace imdj {

bool ReadAudioTags(const std::string& utf8Path, TrackInfo& info)
{
    if (!info.title.empty() && !info.artist.empty()) {
        return false;
    }

    const TagLib::FileRef file(PathFromUtf8(utf8Path).c_str(), false);
    if (file.isNull() || !file.tag()) {
        return false;
    }

    bool filled = false;
    auto fill = [&filled](std::string& field, const TagLib::String& value) {
        if (field.empty() && !value.isEmpty()) {
            field = value.to8Bit(true);
            filled = true;
        }
    };

    fill(info.title, file.tag()->title());
    fill(info.artist, file.tag()->artist());

    return filled;
}

} // namespace imdj
