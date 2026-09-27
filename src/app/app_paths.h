#pragma once

#include <string>

namespace imdj {

std::string AppDataDir();

std::string SessionPresetsDir();

std::string ResourcePath(const std::string& relative);

void OpenFolderInFileManager(const std::string& path);

} // namespace imdj
