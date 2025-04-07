#pragma once

#include <string>
#include <vector>

namespace Loader
{
#if defined(__EMSCRIPTEN__)
    uint32_t loadFile(
        char** pacFileContentBuffer,
        std::string const& filePath,
        bool bTextFile = false);
    void loadFileFree(void* pData);
#else
    void loadFile(
        std::vector<char>& acFileContentBuffer,
        std::string const& filePath,
        bool bTextFile = false);
#endif // __EMSCRIPTEN__

}   // Loader