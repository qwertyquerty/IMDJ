#pragma once

#include <string>
#include <vector>

namespace imdj {

class Notifications {
public:
    void info(std::string message);
    void error(std::string message);

    void errorOnChange(std::string& reported, const std::string& current);

    void draw();

private:
    struct Entry {
        std::string message;
        bool isError = false;
        double postedAt = 0.0;
    };

    std::vector<Entry> entries_;
};

} // namespace imdj
