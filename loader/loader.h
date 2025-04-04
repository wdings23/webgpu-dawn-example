#pragma once

#include <string>
#include <vector>

namespace Loader
{
    void loadFile(
        std::vector<char>& acFileContentBuffer,
        std::string const& filePath,
        bool bTextFile = false);

}   // Loader