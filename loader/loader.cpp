#include <loader/loader.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/fetch.h>
#else
#include <curl/curl.h>
#endif // __EMSCRIPTEN__

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
    /*
    **
    */
    void downloadSucceeded(emscripten_fetch_t* fetch)
    {
        std::vector<char>& acFileContentBuffer = *((std::vector<char>*)fetch->userData);

        acFileContentBuffer.resize(fetch->numBytes);
        memcpy(acFileContentBuffer.data(), fetch->data, fetch->numBytes);
        emscripten_fetch_close(fetch);
    }

    /*
    **
    */
    void downloadFailed(emscripten_fetch_t* fetch)
    {
        printf("!!! error fetching data !!!\n");
        emscripten_fetch_close(fetch);
    }

#endif // __EMSCRIPTEN__

    /*
    **
    */
    void loadFile(
        std::vector<char>& acFileContentBuffer,
        std::string const& filePath,
        bool bTextFile)
    {
        std::string url = "http://127.0.0.1:8080/" + filePath;

#if defined(__EMSCRIPTEN__)
        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "GET");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY; // Load response into memory
        attr.onsuccess = downloadSucceeded;
        attr.onerror = downloadFailed;
        attr.userData = &acFileContentBuffer;

        emscripten_fetch(&attr, url.c_str());
#else 
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
#endif // __EMSCRIPTEN__
    }
}