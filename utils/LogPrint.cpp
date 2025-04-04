#include <stdio.h>
#include <stdarg.h>
#include <vector>
#include "LogPrint.h"
#include <assert.h>
#include <chrono>

#ifdef _MSC_VER
#include <Windows.h>
#endif // _MSC_VER

#ifdef ANDROID
#include <android/log.h>
#endif // ANDROID

#include <string>

#include <mutex>

//#define SAVE_LOG_FILE 1

#if defined(SAVE_LOG_FILE)
static bool sbStarted = false;
extern "C" char const* getSaveDir();
#endif // SAVE_LOG_FILE

static char sacBuffer[65536];
static PrintOptions sOptions;

/*
**
*/
extern "C" int printOutputToDebugWindow(char const* const szFormat, ...)
{
//#if defined(_DEBUG)
	//std::vector<char> aBuffer(1 << 20);
	//char* szBuffer = aBuffer.data();

	va_list args;
	va_start(args, szFormat);
#if defined(_MSC_VER)
    vsprintf(sacBuffer, szFormat, args);
#else
    vsnprintf(sacBuffer, 1024, szFormat, args);
#endif // _MSC_VER
	//perror(szBuffer);
	va_end(args);

    std::string output = "";
    if(sOptions.mbDisplayTime)
    {
        auto timePt = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(timePt);
        std::string timeStr = std::ctime(&time);
        std::string substr = std::string(timeStr.begin(), timeStr.end() - 1);
        output += substr + " ";
    }
   
    output += std::string(sacBuffer);

#ifdef _MSC_VER
    {
        OutputDebugStringA(output.c_str());
    }
#else
	#ifdef ANDROID
		__android_log_print(ANDROID_LOG_VERBOSE, "RenderWithMe", "%s", szBuffer); 
	#else 
    	printf("%s", sacBuffer);
	#endif // ANDROID
#endif // _MSC_VER
//#endif // _DEBUG

#if defined(SAVE_LOG_FILE)
    std::string dir(getSaveDir());
    std::string fullPath = dir + "/log.txt";
    
    FILE* fp = nullptr;
    if(sbStarted)
    {
        fp = fopen(fullPath.c_str(), "ab");
    }
    else
    {
        fp = fopen(fullPath.c_str(), "wb");
        sbStarted = true;
    }
    
    fprintf(fp, "%s", output.c_str());
    fclose(fp);
#endif // SAVE_LOG_FILE
    
	return 0;
}

/*
**
*/
void convertWChar(
    char* acBuffer,
    wchar_t const* awcBuffer,
    uint32_t iMaxLength)
{
    uint32_t iNumChar = 0;
    wchar_t const* pWChar = awcBuffer;
    for(uint32_t j = 0; j < iMaxLength - 1; j++)
    {
        if(pWChar == nullptr || *pWChar == 0)
        {
            break;
        }

        wctomb(&acBuffer[iNumChar], *pWChar);
        ++iNumChar;
        ++pWChar;
    }
    acBuffer[iNumChar] = '\0';
}

/*
**
*/
extern "C" void setPrintOptions(PrintOptions options)
{
    sOptions = options;
}
