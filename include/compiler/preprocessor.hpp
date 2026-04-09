#pragma once

#include <string>

namespace cd {

class Preprocessor {
public:
    // Removes comments while preserving newlines and text width for stable line/column mapping.
    std::string run(const std::string& source) const;
};

}  // namespace cd
