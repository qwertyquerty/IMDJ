#pragma once

#include <string>
#include <vector>

#include "audio/track_info.h"

namespace imdj {

struct SongEntry {
    std::string path;
    std::string label;
    TrackInfo info;
};

struct FolderNode {
    std::string name;
    std::string fullPath;
    std::vector<FolderNode> children;
};

enum class SongSortColumn { Track = 0, Bpm = 1, Key = 2 };

struct SongLibrary {
    std::vector<std::string> directories;
    std::vector<SongEntry> entries;
    std::vector<FolderNode> folderTree;
    std::string selectedFolderPath;
    char searchBuffer[256] = "";
    char newDirBuffer[512] = "";
    std::string lastScanError;
    std::string previewError;

    bool autoRescan = false;
    float autoRescanIntervalSeconds = 5.0f;
    double lastAutoRescanTime = 0.0;

    SongSortColumn sortColumn = SongSortColumn::Track;
    bool sortAscending = true;
    bool sortDirty = true;

    void applySort();
    void maybeAutoRescan(double now);

    void setCachedInfo(const std::string& path, const TrackInfo& info);

    void addDirectory(const std::string& dir);
    void removeDirectory(size_t index);
    void rescan();

    bool matches(const SongEntry& song, const std::string& lowercaseFilter) const;
};

bool PathUnderFolder(const std::string& filePath, const std::string& folderPath);

} // namespace imdj
