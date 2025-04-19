#include <loader/loader.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/fetch.h>
#include <curl/curl.h>
#else
#include <curl/curl.h>
#endif // __EMSCRIPTEN__

#include <assert.h>

#include <tinyexr/miniz.h>

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
        //gpFetch = fetch;
        gacTempMemory = (char*)malloc(fetch->numBytes + 1);
        memcpy(gacTempMemory, fetch->data, fetch->numBytes);
        *(gacTempMemory + fetch->numBytes) = 0;
        giTempMemorySize = fetch->numBytes;
        printf("setting %d bytes\n", giTempMemorySize);

        emscripten_fetch_close(fetch);

        bDoneLoading = true;
    }

    /*
    **
    */
    void downloadFailed(emscripten_fetch_t* fetch)
    {
        printf("!!! error fetching data !!!\n");
        emscripten_fetch_close(fetch);

        giTempMemorySize = 0;

        bDoneLoading = true;
    }
#endif // __EMSCRIPTEN__

#if defined(__EMSCRIPTEN__)

#if defined(EMBEDDED_FILES)
    uint32_t loadFile(
        char** pacFileContentBuffer,
        std::string const& filePath,
        bool bTextFile)
    {
        printf("load %s\n", filePath.c_str());
        
        auto fileExtensionStart = filePath.rfind(".") - 1;
        auto directoryEnd = filePath.rfind("/");
        std::string baseName = filePath.substr(directoryEnd + 1, fileExtensionStart - directoryEnd);

        bool bZipFile = false;
        FILE* fp = fopen(filePath.c_str(), "rb");
        if(fp == nullptr)
        {
            std::string newPath = std::string("assets/") + filePath;
            fp = fopen(newPath.c_str(), "rb");
            printf("new path: %s\n", newPath.c_str());

            if(fp == nullptr)
            {
                printf("%s : %d base name: %s\n", 
                    __FILE__,
                    __LINE__,
                    baseName.c_str());

                newPath = std::string("assets/") + baseName + ".zip";
                fp = fopen(newPath.c_str(), "rb");
                printf("new path: %s\n", newPath.c_str());

                bZipFile = true;
            }
        }
        assert(fp);

        fseek(fp, 0, SEEK_END);
        size_t iFileSize = ftell(fp);
        if(bTextFile)
        {
            iFileSize += 1;
        }
        fseek(fp, 0, SEEK_SET);
        
        printf("file size: %d\n", (uint32_t)iFileSize);

        gacTempMemory = (char*)malloc(iFileSize);
        fread(gacTempMemory, sizeof(char), iFileSize, fp);
        if(bTextFile)
        {
            *(gacTempMemory + (iFileSize - 1)) = 0;
        }
        giTempMemorySize = iFileSize;

        fclose(fp);

        if(bZipFile)
        {
            std::string fileName = baseName + ".bin";
            printf("%s : %d zip file to extract: %s\n",
                __FILE__,
                __LINE__,
                fileName.c_str());

            mz_zip_archive zipArchive;
            memset(&zipArchive, 0, sizeof(zipArchive));

            mz_bool status = mz_zip_reader_init_mem(&zipArchive, gacTempMemory, giTempMemorySize, 0);
            if(!status)
            {
                printf("%s : %d status = %d\n",
                    __FILE__,
                    __LINE__,
                    status);
                assert(0);
            }

            int32_t iFileIndex = mz_zip_reader_locate_file(&zipArchive, fileName.c_str(), nullptr, 0);
            if(iFileIndex == -1)
            {
                printf("%s : %d can\'t find file \"%s\"\n",
                    __FILE__,
                    __LINE__,
                    fileName.c_str());
                assert(0);
            }
            printf("%s : %d file index = %d\n",
                __FILE__,
                __LINE__,
                iFileIndex);

            size_t iUncompressedSize;
            void* buffer = mz_zip_reader_extract_file_to_heap(&zipArchive, fileName.c_str(), &iUncompressedSize, 0);
            if(buffer == nullptr)
            {
                printf("%s : %d can\'t uncompress file \"%s\"\n",
                    __FILE__,
                    __LINE__,
                    fileName.c_str());
                assert(0);
            }
            free(gacTempMemory);
            gacTempMemory = (char*)malloc(iUncompressedSize);
            memcpy(gacTempMemory, buffer, iUncompressedSize);
            giTempMemorySize = (uint32_t)iUncompressedSize;

            mz_free(buffer);
            mz_zip_reader_end(&zipArchive);

            printf("%s : %d \"%s\" uncompressed size: %d\n",
                __FILE__,
                __LINE__,
                fileName.c_str(),
                (uint32_t)iUncompressedSize);
        }

        *pacFileContentBuffer = gacTempMemory;

        return (uint32_t)iFileSize;
    }
#else
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

        //*pacFileContentBuffer = (char*)gpFetch->data;
        *pacFileContentBuffer = gacTempMemory;

        printf("%s : %d returning size %d\n",
            __FILE__,
            __LINE__,
            giTempMemorySize);

        return giTempMemorySize;
    }
#endif // EMBEDDED_FILES

    void loadFileFree(void* pData)
    {
        //emscripten_fetch_close(gpFetch);
        free(gacTempMemory);
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