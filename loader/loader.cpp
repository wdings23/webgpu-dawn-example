#include <loader/loader.h>

#include <curl/curl.h>

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

    /*
    **
    */
    void loadFile(
        std::vector<char>& acFileContentBuffer,
        std::string const& filePath,
        bool bTextFile)
    {
        CURL* curl;
        CURLcode res;

        std::string url = "http://127.0.0.1:8080/" + filePath;
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
}