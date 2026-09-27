#pragma once

#include <functional>

namespace imdj {

// Only catches crashes on MSVC builds
bool RunGuarded(const std::function<void()>& fn);

} // namespace imdj
