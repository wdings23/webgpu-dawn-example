#include <loader/loader.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/fetch.h>
#include <curl/curl.h>
#else
#include <curl/curl.h>
#endif // __EMSCRIPTEN__

#include <assert.h>

namespace Loader
{
    // Callback function to write data to file
    size_t writeData(void* ptr, size_t size, size_t nmemb, void* pData) {
        size_t iTotalSize = size * nmemb;
        std::vector<char>* pBuffer = (std::vector<char>*)pData;
        uint32_t iPrevSize = (uint32_t)pBuffer->size();
        pBuffer->resize(pBuffer->size() + iTotalSize);
        char* pBufferEnd = pBuffer->data();
        pBufferEnd += iPrevSize;
        memcpy(pBufferEnd, ptr, iTotalSize);

        return iTotalSize;
    }

#if defined(__EMSCRIPTEN__)
    bool bDoneLoading = false;

    char* gacTempMemory = nullptr;
    uint32_t giTempMemorySize = 0;
    emscripten_fetch_t* gpFetch = nullptr;

    /*
    **
    */
    void downloadSucceeded(emscripten_fetch_t* fetch)
    {
        printf("received %llu bytes\n", fetch->numBytes);
        gpFetch = fetch;
        bDoneLoading = true;
    }

    /*
    **
    */
    void downloadFailed(emscripten_fetch_t* fetch)
    {
        printf("!!! error fetching data !!!\n");
        emscripten_fetch_close(fetch);

        bDoneLoading = true;
    }
#endif // __EMSCRIPTEN__

#if defined(__EMSCRIPTEN__)
    uint32_t loadFile(
        char** pacFileContentBuffer,
        std::string const& filePath,
        bool bTextFile)
    {
        std::string url = "http://127.0.0.1:8080/" + filePath;

        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "GET");
        attr.attributes = /*EMSCRIPTEN_FETCH_SYNCHRONOUS | */EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;; // Load response into memory
        attr.onsuccess = downloadSucceeded;
        attr.onerror = downloadFailed;
        attr.userData = (void*)pacFileContentBuffer;

        printf("load %s\n", url.c_str());
        gacTempMemory = nullptr;
        giTempMemorySize = 0;
        emscripten_fetch(&attr, url.c_str());
        
        bDoneLoading = false;
        while(bDoneLoading == false)
        {
            emscripten_sleep(100);
        }

        *pacFileContentBuffer = (char*)gpFetch->data;

        return giTempMemorySize;
    }

    void loadFileFree(void* pData)
    {
        emscripten_fetch_close(gpFetch);
    }

#else 

    /*
    **
    */
    void loadFile(
        std::vector<char>& acFileContentBuffer,
        std::string const& filePath,
        bool bTextFile)
    {
        std::string url = "http://127.0.0.1:8080/" + filePath;

        CURL* curl;
        CURLcode res;

        curl = curl_easy_init();
        if(curl)
        {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeData);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &acFileContentBuffer);
            res = curl_easy_perform(curl);

            curl_easy_cleanup(curl);
        }

        if(bTextFile)
        {
            acFileContentBuffer.push_back(0);
        }
    }
#endif // __EMSCRIPTEN__
}