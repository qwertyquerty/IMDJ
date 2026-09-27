#include <algorithm>
#include <filesystem>

#include "core/paths.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "vst/vst_host.h"

namespace imdj {

std::vector<VstPluginDescriptor> ScanVst3Plugins(const std::string& extraDir)
{
    namespace fs = std::filesystem;
    std::vector<std::string> modulePaths = VST3::Hosting::Module::getModulePaths();

    std::error_code ec;
    if (!extraDir.empty()) {
        for (fs::directory_iterator it(PathFromUtf8(extraDir), fs::directory_options::skip_permission_denied, ec), end;
             !ec && it != end;
             it.increment(ec)) {
            if (HasExtension(it->path(), {".vst3"})) {
                modulePaths.push_back(PathToUtf8(it->path()));
            }
        }
    }

    std::vector<VstPluginDescriptor> plugins;
    for (std::string& modulePath : modulePaths) {
        std::string label = PathToUtf8(PathFromUtf8(modulePath).stem());
        plugins.push_back({std::move(modulePath), std::move(label)});
    }

    std::ranges::sort(plugins, {}, &VstPluginDescriptor::label);
    return plugins;
}

} // namespace imdj
