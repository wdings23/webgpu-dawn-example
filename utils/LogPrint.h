#pragma once

struct PrintOptions
{
    bool        mbDisplayTime = true;
};

#if defined(_DEBUG) || defined(DEBUG)
#define DEBUG_PRINTF(...)	printOutputToDebugWindow(__VA_ARGS__)
#define DEBUG_PRINTF_SET_OPTIONS(X) setPrintOptions(X)
#else
//#define DEBUG_PRINTF(...)	{}
#define DEBUG_PRINTF(...)	printf(__VA_ARGS__)//printOutputToDebugWindow(__VA_ARGS__)
#define DEBUG_PRINTF2(...)  printf(__VA_ARGS__)
#define DEBUG_PRINTF_SET_OPTIONS(X) setPrintOptions(X)
#endif // #if _DEBUG

extern "C" int printOutputToDebugWindow(char const* const szFormat, ...);
extern "C" int printOutputToDebugWindow2(char const* const szFormat, ...);
extern "C" void setPrintOptions(PrintOptions options);

void convertWChar(
    char* acBuffer,
    wchar_t const* awcBuffer,
    uint32_t iMaxLength);