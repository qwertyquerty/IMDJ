#include "library/song_library.h"

#include <algorithm>
#include <filesystem>
#include <optional>

#include "audio/track_metadata.h"
#include "library/audio_tags.h"
#include "core/paths.h"
#include "core/text.h"
#include "core/strings.h"

namespace imdj {

namespace fs = std::filesystem;

namespace {

std::string StripTrailingSeparators(std::string path)
{
    while (!path.empty() && (path.back() == '/' || path.back() == '\\')) {
        path.pop_back();
    }

    return path;
}

FolderNode& ChildNamed(FolderNode& parent, const std::string& name, const fs::path& fullPath)
{
    for (FolderNode& child : parent.children) {
        if (child.name == name) {
            return child;
        }
    }

    FolderNode node;
    node.name = name;
    node.fullPath = PathToUtf8(fullPath);
    parent.children.push_back(std::move(node));

    return parent.children.back();
}

SongEntry MakeEntry(const fs::path& file)
{
    SongEntry entry;
    entry.path = PathToUtf8(file);
    entry.label = PathToUtf8(file.filename());

    TrackMetadata meta;
    if (TrackMetadata::Load(entry.path, meta)) {
        entry.info = meta.info;
    }

    ReadAudioTags(entry.path, entry.info);
    entry.label = entry.info.label(entry.label);
    return entry;
}

} // namespace

bool PathUnderFolder(const std::string& filePath, const std::string& folderPath)
{
    if (folderPath.empty()) {
        return true;
    }

    if (filePath.size() <= folderPath.size() || !filePath.starts_with(folderPath)) {
        return false;
    }

    char separator = filePath[folderPath.size()];
    return separator == '/' || separator == '\\';
}

void SongLibrary::maybeAutoRescan(double now)
{
    if (!autoRescan || now - lastAutoRescanTime < autoRescanIntervalSeconds) {
        return;
    }

    lastAutoRescanTime = now;
    rescan();
}

void SongLibrary::applySort()
{
    auto sortBy = [this](auto key) {
        std::ranges::sort(entries, [&](const SongEntry& a, const SongEntry& b) {
            const auto left = key(a);
            const auto right = key(b);
            if (!left || !right) {
                return left.has_value() && !right.has_value();
            }

            return sortAscending ? *left < *right : *right < *left;
        });
    };

    switch (sortColumn) {
        case SongSortColumn::Bpm:
            sortBy([](const SongEntry& e) { return e.info.bpm > 0.0 ? std::optional(e.info.bpm) : std::nullopt; });
            break;
        case SongSortColumn::Key:
            sortBy([](const SongEntry& e) { return e.info.key >= 0 ? std::optional(e.info.key) : std::nullopt; });
            break;
        case SongSortColumn::Track: sortBy([](const SongEntry& e) { return std::optional(ToLower(e.label)); }); break;
    }

    sortDirty = false;
}

void SongLibrary::setCachedInfo(const std::string& path, const TrackInfo& info)
{
    for (SongEntry& entry : entries) {
        if (entry.path != path) {
            continue;
        }

        entry.info = info;
        entry.label = info.label(PathToUtf8(PathFromUtf8(path).filename()));
        sortDirty = true;

        return;
    }
}

void SongLibrary::addDirectory(const std::string& dir)
{
    if (dir.empty()) {
        return;
    }

    if (std::ranges::find(directories, dir) != directories.end()) {
        return;
    }

    directories.push_back(dir);
    rescan();
}

void SongLibrary::removeDirectory(size_t index)
{
    if (index >= directories.size()) {
        return;
    }

    directories.erase(directories.begin() + static_cast<long>(index));
    rescan();
}

void SongLibrary::rescan()
{
    entries.clear();
    folderTree.clear();
    lastScanError.clear();

    for (const std::string& directory : directories) {
        std::error_code ec;
        fs::path dir(directory);
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
            lastScanError += TrFormat("error.folder_missing", directory) + "\n";
            continue;
        }

        FolderNode root;
        root.name = PathToUtf8(dir.filename());
        if (root.name.empty()) {
            root.name = directory;
        }

        root.fullPath = StripTrailingSeparators(directory);
        bool foundTrack = false;

        try {
            const auto options = fs::directory_options::skip_permission_denied;
            for (const auto& item : fs::recursive_directory_iterator(dir, options)) {
                if (!item.is_regular_file() || !HasExtension(item.path(), {".wav", ".mp3", ".flac"})) {
                    continue;
                }

                entries.push_back(MakeEntry(item.path()));
                foundTrack = true;

                fs::path relative = fs::relative(item.path().parent_path(), dir, ec);
                if (ec) {
                    ec.clear();
                    continue;
                }

                FolderNode* cursor = &root;
                fs::path accumulated = dir;
                for (const fs::path& part : relative) {
                    if (part == ".") {
                        continue;
                    }

                    accumulated /= part;
                    cursor = &ChildNamed(*cursor, PathToUtf8(part), accumulated);
                }
            }
        }
        catch (const std::exception& e) {
            lastScanError += TrFormat("error.folder_scan", directory, e.what()) + "\n";
        }

        if (foundTrack) {
            folderTree.push_back(std::move(root));
        }
    }

    if (!selectedFolderPath.empty()) {
        bool stillPresent = std::ranges::any_of(entries, [this](const SongEntry& entry) {
            return PathUnderFolder(entry.path, selectedFolderPath);
        });
        if (!stillPresent) {
            selectedFolderPath.clear();
        }
    }

    applySort();
}

bool SongLibrary::matches(const SongEntry& song, const std::string& lowercaseFilter) const
{
    if (!lowercaseFilter.empty() && ToLower(song.label).find(lowercaseFilter) == std::string::npos) {
        return false;
    }

    return PathUnderFolder(song.path, selectedFolderPath);
}

} // namespace imdj
