#pragma once

#include <string>

namespace imdj {

std::string SessionValue(const char* key);
void SetSessionValue(const char* key, const std::string& value);

} // namespace imdj
