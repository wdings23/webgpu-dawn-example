#include <render/renderer.h>

#include <curl/curl.h>

#include <rapidjson/document.h>
#include <math/vec.h>
#include <math/mat4.h>
#include <loader/loader.h>
#include <assert.h>

#include <iostream>
#include <string>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif // __EMSCRIPTEN__

//#define TINYEXR_IMPLEMENTATION
//#include <tinyexr/tinyexr.h>
//#include <tinyexr/miniz.c>

#define STB_IMAGE_IMPLEMENTATION
#include <external/stb_image/stb_image.h>

#include <utils/LogPrint.h>

#if !defined(__EMSCRIPTEN__)
#undef max
#endif //__EMSCRIPTEN__

struct Vertex
{
    vec4        mPosition;
    vec4        mUV;
    vec4        mNormal;
};

struct DefaultUniformData
{
    int32_t miScreenWidth = 0;
    int32_t miScreenHeight = 0;
    int32_t miFrame = 0;
    uint32_t miNumMeshes = 0;

    float mfRand0 = 0.0f;
    float mfRand1 = 0.0f;
    float mfRand2 = 0.0f;
    float mfRand3 = 0.0f;

    float4x4 mViewProjectionMatrix;
    float4x4 mPrevViewProjectionMatrix;
    float4x4 mViewMatrix;
    float4x4 mProjectionMatrix;

    float4x4 mJitteredViewProjectionMatrix;
    float4x4 mPrevJitteredViewProjectionMatrix;

    float4 mCameraPosition;
    float4 mCameraLookDir;

    float4 mLightRadiance;
    float4 mLightDirection;

    float mfAmbientOcclusionDistanceThreshold = 0.0f;
};



// Callback function to write data to file
size_t writeData(void* ptr, size_t size, size_t nmemb, void* pData) 
{
    size_t iTotalSize = size * nmemb;
    std::vector<char>* pBuffer = (std::vector<char>*)pData;
    uint32_t iPrevSize = (uint32_t)pBuffer->size();
    pBuffer->resize(pBuffer->size() + iTotalSize);
    char* pBufferEnd = pBuffer->data();
    pBufferEnd += iPrevSize;
    memcpy(pBufferEnd, ptr, iTotalSize);

    return iTotalSize;
}

// Function to download file from URL
void streamBinary(
    const std::string& url,
    std::vector<char>& acTriangleBuffer) 
{
    CURL* curl;
    CURLcode res;


    curl = curl_easy_init();
    if(curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeData);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &acTriangleBuffer);
        res = curl_easy_perform(curl);

        curl_easy_cleanup(curl);
    }
}

namespace Render
{
    /*
    **
    */
    void CRenderer::setup(CreateDescriptor& desc)
    {
        mCreateDesc = desc;

        mpDevice = desc.mpDevice;
        wgpu::Device& device = *mpDevice;

        
#if defined(__EMSCRIPTEN__)
        char* acTriangleBuffer = nullptr;
        uint64_t iSize = Loader::loadFile(&acTriangleBuffer, desc.mMeshFilePath + "-triangles.bin");
        printf("acTriangleBuffer = 0x%X size: %lld\n", (uint32_t)acTriangleBuffer, iSize);
        uint32_t const* piData = (uint32_t const*)acTriangleBuffer;
#else 
        std::vector<char> acTriangleBuffer;
        Loader::loadFile(acTriangleBuffer, desc.mMeshFilePath + "-triangles.bin");
        uint32_t const* piData = (uint32_t const*)acTriangleBuffer.data();
#endif // __EMSCRIPTEN__
        
        uint32_t iNumMeshes = *piData++;
        uint32_t iNumTotalVertices = *piData++;
        uint32_t iNumTotalTriangles = *piData++;
        uint32_t iVertexSize = *piData++;
        uint32_t iTriangleStartOffset = *piData++;

        printf("num meshes: %d\n", iNumMeshes);
        printf("num total vertices: %d\n", iNumTotalVertices);

        // triangle ranges for all the meshes
        maMeshTriangleRanges.resize(iNumMeshes);
        memcpy(maMeshTriangleRanges.data(), piData, sizeof(MeshTriangleRange) * iNumMeshes);
        piData += (2 * iNumMeshes);

        // the total mesh extent is at the very end of the list
        MeshExtent const* pMeshExtent = (MeshExtent const*)piData;
        maMeshExtents.resize(iNumMeshes + 1);
        memcpy(maMeshExtents.data(), pMeshExtent, sizeof(MeshExtent) * (iNumMeshes + 1));
        pMeshExtent += (iNumMeshes + 1);
        mTotalMeshExtent = maMeshExtents.back();

        // all the mesh vertices
        std::vector<Vertex> aTotalMeshVertices(iNumTotalVertices);
        Vertex const* pVertices = (Vertex const*)pMeshExtent;
        memcpy(aTotalMeshVertices.data(), pVertices, iNumTotalVertices * sizeof(Vertex));
        pVertices += iNumTotalVertices;

        // all the triangle indices
        std::vector<uint32_t> aiTotalMeshTriangleIndices(iNumTotalTriangles * 3);
        piData = (uint32_t const*)pVertices;
        memcpy(aiTotalMeshTriangleIndices.data(), piData, iNumTotalTriangles * 3 * sizeof(uint32_t));

#if defined(__EMSCRIPTEN__)
        Loader::loadFileFree(acTriangleBuffer);
#endif // __EMSCRIPTEN__

        wgpu::BufferDescriptor bufferDesc = {};

        bufferDesc.size = iNumTotalVertices * sizeof(Vertex);
        bufferDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        maBuffers["train-vertex-buffer"] = device.CreateBuffer(&bufferDesc);
        maBuffers["train-vertex-buffer"].SetLabel("Train Vertex Buffer");
        maBufferSizes["train-vertex-buffer"] = (uint32_t)bufferDesc.size;

        bufferDesc.size = aiTotalMeshTriangleIndices.size() * sizeof(uint32_t);
        bufferDesc.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        maBuffers["train-index-buffer"] = device.CreateBuffer(&bufferDesc);
        maBuffers["train-index-buffer"].SetLabel("Train Index Buffer");
        maBufferSizes["train-index-buffer"] = (uint32_t)bufferDesc.size;

        bufferDesc.size = iNumTotalVertices * sizeof(Vertex);
        bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        maBuffers["meshTriangleIndexRanges"] = device.CreateBuffer(&bufferDesc);
        maBuffers["meshTriangleIndexRanges"].SetLabel("Mesh Triangle Ranges");
        maBufferSizes["meshTriangleIndexRanges"] = (uint32_t)bufferDesc.size;

        bufferDesc.size = (iNumMeshes + 1) * sizeof(MeshExtent);
        bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        maBuffers["meshExtents"] = device.CreateBuffer(&bufferDesc);
        maBuffers["meshExtents"].SetLabel("Train Mesh Extents");
        maBufferSizes["meshExtents"] = (uint32_t)bufferDesc.size;

        device.GetQueue().WriteBuffer(maBuffers["train-vertex-buffer"], 0, aTotalMeshVertices.data(), iNumTotalVertices * sizeof(Vertex));
        device.GetQueue().WriteBuffer(maBuffers["train-index-buffer"], 0, aiTotalMeshTriangleIndices.data(), aiTotalMeshTriangleIndices.size() * sizeof(uint32_t));
        device.GetQueue().WriteBuffer(maBuffers["meshTriangleIndexRanges"], 0, maMeshTriangleRanges.data(), maMeshTriangleRanges.size() * sizeof(MeshTriangleRange));
        device.GetQueue().WriteBuffer(maBuffers["meshExtents"], 0, maMeshExtents.data(), maMeshExtents.size() * sizeof(MeshExtent));

        {
#if defined(__EMSCRIPTEN__)
            char* acMaterialID = nullptr;
            bufferDesc.size = Loader::loadFile(&acMaterialID, desc.mMeshFilePath + ".mid");
#else 

            std::vector<char> acMaterialID;
            Loader::loadFile(acMaterialID, desc.mMeshFilePath + ".mid");
            bufferDesc.size = acMaterialID.size();
#endif // __EMSCRIPTEN__

            bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
            maBuffers["meshMaterialIDs"] = device.CreateBuffer(&bufferDesc);
            maBuffers["meshMaterialIDs"].SetLabel("Mesh Material IDs");
            maBufferSizes["meshEmeshMaterialIDsxtents"] = (uint32_t)bufferDesc.size;

#if defined(__EMSCRIPTEN__)
            device.GetQueue().WriteBuffer(
                maBuffers["meshMaterialIDs"],
                0,
                acMaterialID,
                bufferDesc.size);
            Loader::loadFileFree(acMaterialID);
#else
            device.GetQueue().WriteBuffer(
                maBuffers["meshMaterialIDs"],
                0,
                acMaterialID.data(),
                acMaterialID.size());
#endif // __EMSCRIPTEN__
        }

        {
#if defined(__EMSCRIPTEN__)
            char* acMaterials = nullptr;
            bufferDesc.size = Loader::loadFile(&acMaterials, desc.mMeshFilePath + ".mat");
            printf("mesh material size: %d\n", (uint32_t)bufferDesc.size);
#else
            std::vector<char> acMaterials;
            Loader::loadFile(acMaterials, desc.mMeshFilePath + ".mat");

            bufferDesc.size = acMaterials.size();
#endif // __EMSCRIPTEN__
            bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
            maBuffers["meshMaterials"] = device.CreateBuffer(&bufferDesc);
            maBuffers["meshMaterials"].SetLabel("Mesh Materials");
            maBufferSizes["meshMaterials"] = (uint32_t)bufferDesc.size;

#if defined(__EMSCRIPTEN__)
            device.GetQueue().WriteBuffer(
                maBuffers["meshMaterials"],
                0,
                acMaterials,
                bufferDesc.size);
            Loader::loadFileFree(acMaterials);
#else 
            device.GetQueue().WriteBuffer(
                maBuffers["meshMaterials"],
                0,
                acMaterials.data(),
                acMaterials.size());
#endif // __EMSCRIPTEN__
        }

        bufferDesc.size = iNumMeshes * sizeof(uint32_t);
        bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        maBuffers["visibilityFlags"] = device.CreateBuffer(&bufferDesc);
        maBuffers["visibilityFlags"].SetLabel("Mesh Visibility Flags");
        maBufferSizes["visibilityFlags"] = (uint32_t)bufferDesc.size;

        // default uniform buffer
        bufferDesc.size = sizeof(DefaultUniformData);
        bufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        maBuffers["default-uniform-buffer"] = device.CreateBuffer(&bufferDesc);
        maBuffers["default-uniform-buffer"].SetLabel("Default Uniform Buffer");
        maBufferSizes["default-uniform-buffer"] = (uint32_t)bufferDesc.size;

        // full screen triangle
        Vertex aFullScreenTriangles[3];
        aFullScreenTriangles[0].mPosition = float4(-1.0f, 3.0f, 0.0f, 1.0f);
        aFullScreenTriangles[0].mNormal = float4(0.0f, 0.0f, 1.0f, 1.0f);
        aFullScreenTriangles[0].mUV = float4(0.0f, -1.0f, 0.0f, 0.0f);

        aFullScreenTriangles[1].mPosition = float4(-1.0f, -1.0f, 0.0f, 1.0f);
        aFullScreenTriangles[1].mNormal = float4(0.0f, 0.0f, 1.0f, 1.0f);
        aFullScreenTriangles[1].mUV = float4(0.0f, 1.0f, 0.0f, 0.0f);

        aFullScreenTriangles[2].mPosition = float4(3.0f, -1.0f, 0.0f, 1.0f);
        aFullScreenTriangles[2].mNormal = float4(0.0f, 0.0f, 1.0f, 1.0f);
        aFullScreenTriangles[2].mUV = float4(2.0f, 1.0f, 0.0f, 0.0f);

        bufferDesc.size = sizeof(Vertex) * 3;
        maBuffers["full-screen-triangle"] = device.CreateBuffer(&bufferDesc);
        maBuffers["full-screen-triangle"].SetLabel("Full Screen Triangle Buffer");
        maBufferSizes["full-screen-triangle"] = (uint32_t)bufferDesc.size;
        device.GetQueue().WriteBuffer(
            maBuffers["full-screen-triangle"], 
            0, 
            aFullScreenTriangles, 
            3 * sizeof(Vertex));

        bufferDesc.size = 256 * sizeof(float2);
        bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        maBuffers["blueNoiseBuffer"] = device.CreateBuffer(&bufferDesc);
        maBuffers["blueNoiseBuffer"].SetLabel("Blue Noise Buffer");
        maBufferSizes["blueNoiseBuffer"] = (uint32_t)bufferDesc.size;

        mpSampler = desc.mpSampler;
        
        struct TextureAtlasInfo
        {
            uint2               miTextureCoord;
            float2              mUV;
            uint32_t            miTextureID;
            uint32_t            miImageWidth;
            uint32_t            miImageHeight;
            uint32_t            miPadding0;
        };

        std::vector<TextureAtlasInfo> aTextureAtlasInfo;
        std::vector<std::string> aDiffuseTextureNames;
        std::vector<std::string> aEmissiveTextureNames;
        std::vector<std::string> aSpecularTextureNames;
        std::vector<std::string> aNormalTextureNames;
        {
            // diffuse texture atlas
            int32_t iAtlasImageWidth = 8192;
            int32_t iAtlasImageHeight = 8192;
            wgpu::TextureFormat aViewFormats[] = {wgpu::TextureFormat::RGBA8Unorm};
            wgpu::TextureDescriptor textureDesc = {};
            textureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
            textureDesc.dimension = wgpu::TextureDimension::e2D;
            textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
            textureDesc.mipLevelCount = 1;
            textureDesc.sampleCount = 1;
            textureDesc.size.depthOrArrayLayers = 1;
            textureDesc.size.width = iAtlasImageWidth;
            textureDesc.size.height = iAtlasImageHeight;
            textureDesc.viewFormatCount = 1;
            textureDesc.viewFormats = aViewFormats;
            mDiffuseTextureAtlas = device.CreateTexture(&textureDesc);

            
#if defined(__EMSCRIPTEN__)
            char* acTextureNames = nullptr;
            uint32_t iSize = Loader::loadFile(&acTextureNames, desc.mMeshFilePath + "-texture-names.tex");
            if(iSize > 0)
#else 
            std::vector<char> acTextureNames;
            Loader::loadFile(acTextureNames, desc.mMeshFilePath + "-texture-names.tex");
            if(acTextureNames.size() > 0)
#endif // __EMSCRIPTEN__
            {
                uint32_t iDiffuseSignature = ('D') | ('F' << 8) | ('S' << 16) | ('E' << 24);
                uint32_t iEmissiveSignature = ('E') | ('M' << 8) | ('S' << 16) | ('V' << 24);
                uint32_t iSpecularSignature = ('S') | ('P' << 8) | ('C' << 16) | ('L' << 24);
                uint32_t iNormalSignature = ('N') | ('R' << 8) | ('M' << 16) | ('L' << 24);

#if defined(__EMSCRIPTEN__)
                uint32_t const* piData = (uint32_t const*)acTextureNames;
                char const* pcEnd = ((char const*)piData) + iSize;
#else 
                uint32_t const* piData = (uint32_t const*)acTextureNames.data();
                char const* pcEnd = ((char const*)piData) + acTextureNames.size();
#endif // __EMSCRIPTEN__
                for(uint32_t iType = 0; iType < 4; iType++)
                {
                    uint32_t iSignature = *piData++;
                    uint32_t iNumTextures = *piData++;
                    char const* pcChar = (char const*)piData;
                    for(uint32_t i = 0; i < iNumTextures; i++)
                    {
                        std::vector<char> acName;
                        while(*pcChar != '\0')
                        {
                            acName.push_back(*pcChar++);
                        }
                        acName.push_back(*pcChar++);

                        std::string convertedName = std::string(acName.data());
                        auto iter = convertedName.rfind("/");
                        if(iter == std::string::npos)
                        {
                            iter = convertedName.rfind("\\");
                        }

                        std::string baseName = convertedName;
                        if(iter != std::string::npos)
                        {
                            baseName = convertedName.substr(iter);
                        }
                        iter = baseName.rfind(".");
                        std::string noExtension = baseName.substr(0, iter);
                        noExtension += ".png";

                        if(iSignature == iDiffuseSignature)
                        {
                            aDiffuseTextureNames.push_back(noExtension);
                        }
                        else if(iSignature == iEmissiveSignature)
                        {
                            aEmissiveTextureNames.push_back(noExtension);
                        }
                        else if(iSignature == iSpecularSignature)
                        {
                            aSpecularTextureNames.push_back(noExtension);
                        }
                        else if(iSignature == iNormalSignature)
                        {
                            aNormalTextureNames.push_back(noExtension);
                        }
                    }
                    piData = (uint32_t const*)pcChar;
                    if(pcChar == pcEnd)
                    {
                        break;
                    }

                }   // for texture type

#if defined(__EMSCRIPTEN__)
                free(acTextureNames);
#endif // __EMSCRIPTEN__

                int32_t iAtlasIndex = 0;
                int32_t iX = 0, iY = 0;
                int32_t iLargestHeight = 0;

                auto copyToAtlas = [&](
                    int32_t& iX,
                    int32_t& iY,
                    int32_t iAtlasImageWidth,
                    int32_t iAtlasImageHeight,
                    std::string const& textureName,
                    wgpu::Texture& textureAtlas,
                    int32_t& iLargestHeight)
                    {
                        std::string parsedTextureName = std::string("textures/") + textureName;

#if defined(__EMSCRIPTEN__)
                        char* acTextureImageData = nullptr;
                        uint32_t iSize = Loader::loadFile(&acTextureImageData, parsedTextureName);
                        int32_t iImageWidth = 0, iImageHeight = 0, iImageComp = 0;
                        stbi_uc* pImageData = stbi_load_from_memory(
                            (stbi_uc const*)acTextureImageData,
                            (int32_t)iSize,
                            &iImageWidth,
                            &iImageHeight,
                            &iImageComp,
                            4
                        );
#else
                        std::vector<char> acTextureImageData;
                        Loader::loadFile(acTextureImageData, parsedTextureName);
                        int32_t iImageWidth = 0, iImageHeight = 0, iImageComp = 0;
                        stbi_uc* pImageData = stbi_load_from_memory(
                            (stbi_uc const*)acTextureImageData.data(),
                            (int32_t)acTextureImageData.size(),
                            &iImageWidth,
                            &iImageHeight,
                            &iImageComp,
                            4
                        );
#endif // __EMSCRIPTEN__

                        if(pImageData)
                        {
                            iLargestHeight = std::max(iLargestHeight, iImageHeight);
                            if(iX + iImageWidth >= iAtlasImageWidth)
                            {
                                iX = 0;
                                iY += iLargestHeight;
                                iLargestHeight = 0;
                            }

#if defined(__EMSCRIPTEN__)
                            wgpu::TextureDataLayout layout = {};
#else
                            wgpu::TexelCopyBufferLayout layout = {};
#endif // __EMSCRIPTEN__
                            layout.bytesPerRow = iImageWidth * 4 * sizeof(char);
                            layout.offset = 0;
                            layout.rowsPerImage = iImageHeight;
                            wgpu::Extent3D extent = {};
                            extent.depthOrArrayLayers = 1;
                            extent.width = iImageWidth;
                            extent.height = iImageHeight;

#if defined(__EMSCRIPTEN__)
                            wgpu::ImageCopyTexture destination = {};
#else 
                            wgpu::TexelCopyTextureInfo destination = {};
#endif // __EMSCRIPTEN__
                            destination.aspect = wgpu::TextureAspect::All;
                            destination.mipLevel = 0;
                            destination.origin = {.x = (uint32_t)iX, .y = (uint32_t)iY, .z = 0};
                            destination.texture = textureAtlas;
                            device.GetQueue().WriteTexture(
                                &destination,
                                pImageData,
                                iImageWidth * iImageHeight * 4,
                                &layout,
                                &extent);

                            TextureAtlasInfo info = {};
                            info.miTextureCoord = uint2(iX, iY);
                            info.miTextureID = iAtlasIndex;
                            info.mUV = float2(float(iX) / float(iAtlasImageWidth), float(iY) / float(iAtlasImageHeight));
                            info.miImageWidth = iImageWidth;
                            info.miImageHeight = iImageHeight;
                            aTextureAtlasInfo.push_back(info);

                            iX += iImageWidth;

                            stbi_image_free(pImageData);

#if defined(__EMSCRIPTEN__)
                            free(acTextureImageData);
#endif // __EMSCRIPTEN__
                        }
                    };


                for(auto const& diffuseTextureName : aDiffuseTextureNames)
                {
                    copyToAtlas(iX, iY, iAtlasImageWidth, iAtlasImageHeight, diffuseTextureName, mDiffuseTextureAtlas, iLargestHeight);

                    ++iAtlasIndex;
                }

            }

        }   // textures

        wgpu::TextureViewDescriptor viewDesc = {};
        viewDesc.arrayLayerCount = 1;
        viewDesc.aspect = wgpu::TextureAspect::All;
        viewDesc.baseArrayLayer = 0;
        viewDesc.baseMipLevel = 0;
        viewDesc.dimension = wgpu::TextureViewDimension::e2D;
        viewDesc.format = wgpu::TextureFormat::RGBA8Unorm;
        viewDesc.label = "Diffuse Texture Atlas";
        viewDesc.mipLevelCount = 1;
#if !defined(__EMSCRIPTEN__)
        viewDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
#endif // __EMSCRIPTEN__
        mDiffuseTextureAtlasView = mDiffuseTextureAtlas.CreateView(&viewDesc);

        bufferDesc = {};
        bufferDesc.mappedAtCreation = false;
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
        bufferDesc.size = std::max((uint32_t)sizeof(TextureAtlasInfo) * (uint32_t)aTextureAtlasInfo.size(), 64u);
        maBuffers["diffuseTextureAtlasInfoBuffer"] = mpDevice->CreateBuffer(&bufferDesc);
        maBuffers["diffuseTextureAtlasInfoBuffer"].SetLabel("Diffuse Texture Atlas Info Buffer");
        device.GetQueue().WriteBuffer(
            maBuffers["diffuseTextureAtlasInfoBuffer"],
            0,
            aTextureAtlasInfo.data(),
            sizeof(TextureAtlasInfo)* (uint32_t)aTextureAtlasInfo.size()
        );

        // font atlas
        {
#if defined(__EMSCRIPTEN__)
            char* acAtlasImageData = nullptr;
            uint32_t iFileSize = Loader::loadFile(&acAtlasImageData, "font-atlas.png");
#else 
            std::vector<char> acAtlasImageDataV;
            Loader::loadFile(acAtlasImageDataV, "font-atlas.png");
            char* acAtlasImageData = acAtlasImageDataV.data();
            uint32_t iFileSize = (uint32_t)acAtlasImageDataV.size();
#endif // __EMSCRIPTEN__

            int32_t iImageWidth = 0, iImageHeight = 0, iNumComp = 0;
            stbi_uc* pImageData = stbi_load_from_memory(
                (stbi_uc const*)acAtlasImageData,
                (int32_t)iFileSize,
                &iImageWidth,
                &iImageHeight,
                &iNumComp,
                4
            );

            wgpu::TextureFormat aViewFormats[] = {wgpu::TextureFormat::RGBA8Unorm};
            wgpu::TextureDescriptor textureDesc = {};
            textureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
            textureDesc.dimension = wgpu::TextureDimension::e2D;
            textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
            textureDesc.mipLevelCount = 1;
            textureDesc.sampleCount = 1;
            textureDesc.size.depthOrArrayLayers = 1;
            textureDesc.size.width = iImageWidth;
            textureDesc.size.height = iImageHeight;
            textureDesc.viewFormatCount = 1;
            textureDesc.viewFormats = aViewFormats;
            maTextures["font-atlas-image"] = mpDevice->CreateTexture(&textureDesc);
            maTextures["font-atlas-image"].SetLabel("Font Atlas");

#if defined(__EMSCRIPTEN__)
            wgpu::TextureDataLayout layout = {};
#else
            wgpu::TexelCopyBufferLayout layout = {};
#endif // __EMSCRIPTEN__
            layout.bytesPerRow = iImageWidth * 4 * sizeof(char);
            layout.offset = 0;
            layout.rowsPerImage = iImageHeight;
            wgpu::Extent3D extent = {};
            extent.depthOrArrayLayers = 1;
            extent.width = iImageWidth;
            extent.height = iImageHeight;

#if defined(__EMSCRIPTEN__)
            wgpu::ImageCopyTexture destination = {};
#else 
            wgpu::TexelCopyTextureInfo destination = {};
#endif // __EMSCRIPTEN__
            destination.aspect = wgpu::TextureAspect::All;
            destination.mipLevel = 0;
            destination.origin = {.x = 0, .y = 0, .z = 0, };
            destination.texture = maTextures["font-atlas-image"];
            device.GetQueue().WriteTexture(
                &destination,
                pImageData,
                iImageWidth * iImageHeight * 4,
                &layout,
                &extent);
            stbi_image_free(pImageData);

            wgpu::TextureViewDescriptor viewDesc = {};
            viewDesc.arrayLayerCount = 1;
            viewDesc.aspect = wgpu::TextureAspect::All;
            viewDesc.baseArrayLayer = 0;
            viewDesc.baseMipLevel = 0;
            viewDesc.dimension = wgpu::TextureViewDimension::e2D;
            viewDesc.format = wgpu::TextureFormat::RGBA8Unorm;
            viewDesc.label = "Font Texture Atlas";
            viewDesc.mipLevelCount = 1;
#if !defined(__EMSCRIPTEN__)
            viewDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
#endif // __EMSCRIPTEN__
            maTextureViews["font-atlas-image"] = maTextures["font-atlas-image"].CreateView(&viewDesc);

#if defined(__EMSCRIPTEN__)
            free(acAtlasImageData);
#endif // __EMSCRIPTEN__

#if defined(__EMSCRIPTEN__)
            char* acFontInfoData = nullptr;
            iFileSize = Loader::loadFile(&acFontInfoData, "glyph_info.bin");
#else 
            std::vector<char> acFontInfoDataV;
            Loader::loadFile(acFontInfoDataV, "glyph_info.bin");
            char* acFontInfoData = acFontInfoDataV.data();
            iFileSize = (uint32_t)acFontInfoDataV.size();
#endif // __EMSCRIPTEN__

            bufferDesc.size = iFileSize;
            bufferDesc.usage = (wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage);
            maBuffers["font-info"] = mpDevice->CreateBuffer(&bufferDesc);
            maBuffers["font-info"].SetLabel("Font Info Buffer");
            mpDevice->GetQueue().WriteBuffer(maBuffers["font-info"], 0, acFontInfoData, iFileSize);

            uint32_t iNumFontInfo = iFileSize / sizeof(OutputGlyphInfo);
            maFontInfo.resize(iNumFontInfo);
            memcpy(maFontInfo.data(), acFontInfoData, iFileSize);

#if defined(__EMSCRIPTEN__)
            free(acFontInfoData);
#endif // __EMSCRIPTEN__

            bufferDesc.size = sizeof(Vertex) * 4;
            bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
            maBuffers["quad-vertex-buffer"] = mpDevice->CreateBuffer(&bufferDesc);
            maBuffers["quad-vertex-buffer"].SetLabel("Glyph Quad Vertex Buffer");

            Vertex aVertices[4];
            aVertices[0].mPosition = float4(-1.0f, 1.0f, 0.0f, 1.0f);
            aVertices[1].mPosition = float4(-1.0f, -1.0f, 0.0f, 1.0f);
            aVertices[2].mPosition = float4(1.0f, -1.0f, 0.0f, 1.0f);
            aVertices[3].mPosition = float4(1.0f, 1.0f, 0.0f, 1.0f);
            aVertices[0].mNormal = float4(0.0f, 0.0f, 1.0f, 1.0f);
            aVertices[1].mNormal = float4(0.0f, 0.0f, 1.0f, 1.0f);
            aVertices[2].mNormal = float4(0.0f, 0.0f, 1.0f, 1.0f);
            aVertices[3].mNormal = float4(0.0f, 0.0f, 1.0f, 1.0f);
            aVertices[0].mUV = float4(0.0f, 0.0f, 0.0f, 1.0f);
            aVertices[1].mUV = float4(0.0f, 1.0f, 0.0f, 1.0f);
            aVertices[2].mUV = float4(1.0f, 1.0f, 0.0f, 1.0f);
            aVertices[3].mUV = float4(1.0f, 0.0f, 0.0f, 1.0f);
            mpDevice->GetQueue().WriteBuffer(maBuffers["quad-vertex-buffer"], 0, aVertices, sizeof(aVertices));

            bufferDesc.size = sizeof(uint32_t) * 6;
            bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index;
            maBuffers["quad-index-buffer"] = mpDevice->CreateBuffer(&bufferDesc);
            maBuffers["quad-index-buffer"].SetLabel("Glyph Quad Index Buffer");

            uint32_t aiIndices[6] = {0, 1, 2, 3, 2, 0};
            mpDevice->GetQueue().WriteBuffer(maBuffers["quad-index-buffer"], 0, aiIndices, sizeof(aiIndices));

            bufferDesc.size = 1024;
            bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
            maBuffers["glyph-coordinates"] = mpDevice->CreateBuffer(&bufferDesc);
            maBuffers["glyph-coordinates"].SetLabel("Glyph Coordinates");

            bufferDesc.size = 64;
            bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
            maBuffers["draw-text-uniform"] = mpDevice->CreateBuffer(&bufferDesc);
            maBuffers["draw-text-uniform"].SetLabel("Draw Text Uniform Buffer");

            setupFontPipeline();
        }

        createRenderJobs(desc);

        struct UniformData
        {
            uint32_t    miNumMeshes;
            float       mfExplodeMultipler;
        };

        UniformData uniformData;
        uniformData.miNumMeshes = (uint32_t)maMeshExtents.size();
        uniformData.mfExplodeMultipler = 1.0f;
        device.GetQueue().WriteBuffer(
            maRenderJobs["Mesh Culling Compute"]->mUniformBuffers["uniformBuffer"],
            0,
            &uniformData,
            sizeof(UniformData));
        
        bufferDesc = {};
        bufferDesc.mappedAtCreation = false;
        bufferDesc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
        bufferDesc.size = 1024;
        mOutputImageBuffer = mpDevice->CreateBuffer(&bufferDesc);
        mOutputImageBuffer.SetLabel("Read Back Image Buffer");

        mLastTimeStart = std::chrono::high_resolution_clock::now();

        mpInstance = desc.mpInstance;
    }

    /*
    **
    */
    void CRenderer::draw(DrawUpdateDescriptor& desc)
    {
        DefaultUniformData defaultUniformData;
        defaultUniformData.mViewMatrix = *desc.mpViewMatrix;
        defaultUniformData.mProjectionMatrix = *desc.mpProjectionMatrix;
        defaultUniformData.mViewProjectionMatrix = *desc.mpViewProjectionMatrix;
        defaultUniformData.mPrevViewProjectionMatrix = *desc.mpPrevViewProjectionMatrix;
        defaultUniformData.mJitteredViewProjectionMatrix = *desc.mpViewProjectionMatrix;
        defaultUniformData.mPrevJitteredViewProjectionMatrix = *desc.mpPrevViewProjectionMatrix;
        defaultUniformData.miScreenWidth = (int32_t)mCreateDesc.miScreenWidth;
        defaultUniformData.miScreenHeight = (int32_t)mCreateDesc.miScreenHeight;
        defaultUniformData.miFrame = miFrame;
        defaultUniformData.mCameraPosition = float4(mCameraPosition, 1.0f);
        defaultUniformData.mCameraLookDir = float4(mCameraLookAt, 1.0f);
        defaultUniformData.miNumMeshes = (uint32_t)maMeshTriangleRanges.size();

        // update default uniform buffer
        mpDevice->GetQueue().WriteBuffer(
            maBuffers["default-uniform-buffer"],
            0,
            &defaultUniformData,
            sizeof(defaultUniformData)
        );

        // clear number of draw calls
        char acClearData[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        mpDevice->GetQueue().WriteBuffer(
            maRenderJobs["Mesh Culling Compute"]->mOutputBufferAttachments["Num Draw Calls"],
            0,
            acClearData,
            sizeof(acClearData)
        );

        for(auto queuedData : maQueueData)
        {
            mpDevice->GetQueue().WriteBuffer(
                maRenderJobs[queuedData.mJobName]->mUniformBuffers[queuedData.mShaderResourceName],
                queuedData.miStart,
                queuedData.mpData,
                queuedData.miSize
            );
        }

        maQueueData.clear();

        struct MeshSelectionUniformData
        {
            int32_t miSelectedMesh;
            int32_t miSelectionX;
            int32_t miSelectionY;
        };

        // fill out uniform data for buffer for highlighting mesh
        {
            if(mCaptureImageJobName.length() > 0)
            {
                // start selection, inform the shader to start looking for selected mesh at coordinate

                MeshSelectionUniformData uniformBuffer;
                uniformBuffer.miSelectionX = mSelectedCoord.x;
                uniformBuffer.miSelectionY = mSelectedCoord.y;
                uniformBuffer.miSelectedMesh = -1;

                mpDevice->GetQueue().WriteBuffer(
                    maRenderJobs["Mesh Selection Graphics"]->mUniformBuffers["uniformBuffer"],
                    0,
                    &uniformBuffer,
                    sizeof(MeshSelectionUniformData)
                );

                mbWaitingForMeshSelection = true;
                mSelectedCoord = int2(-1, -1);
                
                if(mCaptureImageJobName.length() > 0 && miFrame - miStartCaptureFrame > 3)
                {
                    miStartCaptureFrame = miFrame;
                }

                mbUpdateUniform = false;
            }
        }
        
        if(mCaptureImageJobName.length() > 0 && mbSelectedBufferCopied)
        {
#if defined(__EMSCRIPTEN__)
            static bool bTestMapped;
            bTestMapped = false;
            wgpu::BufferMapCallback callBack = [](WGPUBufferMapAsyncStatus status, void* userData)
                {
                    printf("buffer mapped\n");
                    bTestMapped = true;
                };
            mOutputImageBuffer.MapAsync(wgpu::MapMode::Read, 0, sizeof(SelectMeshInfo), callBack, nullptr);
            while(bTestMapped == false)
            {
                emscripten_sleep(10);
            }
            SelectMeshInfo const* pInfo = (SelectMeshInfo const*)mOutputImageBuffer.GetConstMappedRange(0, sizeof(SelectMeshInfo));
            memcpy(&mSelectMeshInfo, pInfo, sizeof(SelectMeshInfo));
            mSelectMeshInfo.miMeshID -= 1;
            mSelectedCoord.x = pInfo->miSelectionCoordX;
            mSelectedCoord.y = pInfo->miSelectionCoordY;
            printf("selected mesh id: %d (%d, %d)\n", 
                pInfo->miMeshID,
                pInfo->miSelectionCoordX,
                pInfo->miSelectionCoordY);
            mOutputImageBuffer.Unmap();

            mCaptureImageJobName = "";
            mCaptureImageName = "";
            mCaptureUniformBufferName = "";
            mbWaitingForMeshSelection = false;
            mbSelectedBufferCopied = false;
#else 
            auto callBack = [&](wgpu::MapAsyncStatus status, const char* message)
            {
                if(status == wgpu::MapAsyncStatus::Success)
                {
                    wgpu::BufferMapState mapState = mOutputImageBuffer.GetMapState();

                    SelectMeshInfo const* pInfo = (SelectMeshInfo const*)mOutputImageBuffer.GetConstMappedRange(0, sizeof(SelectMeshInfo));
                    assert(pInfo != nullptr);
                    memcpy(&mSelectMeshInfo, pInfo, sizeof(SelectMeshInfo));
                    mSelectMeshInfo.miMeshID -= 1;
                    mOutputImageBuffer.Unmap();

                    mCaptureImageJobName = "";
                    mCaptureImageName = "";
                    mCaptureUniformBufferName = "";
                    mbWaitingForMeshSelection = false;
                    mbSelectedBufferCopied = false;

                    DEBUG_PRINTF("!!! selected mesh: %d coordinate (%d, %d) min (%.4f, %.4f, %.4f) max(%.4f, %.4f, %.4f) !!!\n",
                        pInfo->miMeshID,
                        pInfo->miSelectionCoordX,
                        pInfo->miSelectionCoordY,
                        pInfo->mMinPosition.x,
                        pInfo->mMinPosition.y,
                        pInfo->mMinPosition.z,
                        pInfo->mMaxPosition.x,
                        pInfo->mMaxPosition.y,
                        pInfo->mMaxPosition.z);
                }
            };

            // read back mesh selection buffer from shader
            uint32_t iFileSize = sizeof(SelectMeshInfo);
            wgpu::Future future = mOutputImageBuffer.MapAsync(
                wgpu::MapMode::Read,
                0,
                iFileSize,
                wgpu::CallbackMode::WaitAnyOnly,
                callBack);

            assert(mpInstance);
            mpInstance->WaitAny(future, UINT64_MAX);

#endif // __EMSCRIPTEN__

            MeshSelectionUniformData uniformBuffer;
            mSelectedCoord.x = mSelectedCoord.y = -1;
            uniformBuffer.miSelectionX = mSelectedCoord.x;
            uniformBuffer.miSelectionY = mSelectedCoord.y;
            uniformBuffer.miSelectedMesh = mSelectMeshInfo.miMeshID;

            printf("uniform selected mesh = %d\n", uniformBuffer.miSelectedMesh);
            mpDevice->GetQueue().WriteBuffer(
                maRenderJobs["Mesh Selection Graphics"]->mUniformBuffers["uniformBuffer"],
                0,
                &uniformBuffer,
                sizeof(MeshSelectionUniformData)
            );
            printf("updated mesh selection uniform\n");

        }

        uint64_t iElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - mLastTimeStart).count();
        mLastTimeStart = std::chrono::high_resolution_clock::now();

        uint32_t iFPS = uint32_t(float(1000.0f) / float(iElapsed));

        std::ostringstream oss;
        oss << iFPS << " fps";

        std::vector<wgpu::CommandBuffer> aCommandBuffer;
        drawText(
            aCommandBuffer,
            oss.str(),
            100,
            20,
            50,
            float3(1.0f, 0.5f, 0.2f)
        );

        // add commands from the render jobs
        for(auto const& renderJobName : maOrderedRenderJobs)
        {
            Render::CRenderJob* pRenderJob = maRenderJobs[renderJobName].get();

            wgpu::CommandEncoderDescriptor commandEncoderDesc = {};
            wgpu::CommandEncoder commandEncoder = mpDevice->CreateCommandEncoder(&commandEncoderDesc);
            if(pRenderJob->mType == Render::JobType::Graphics)
            {
                wgpu::RenderPassDescriptor renderPassDesc = {};
                renderPassDesc.colorAttachmentCount = pRenderJob->maOutputAttachments.size();
                renderPassDesc.colorAttachments = pRenderJob->maOutputAttachments.data();
                renderPassDesc.depthStencilAttachment = &pRenderJob->mDepthStencilAttachment;
                wgpu::RenderPassEncoder renderPassEncoder = commandEncoder.BeginRenderPass(&renderPassDesc);

                renderPassEncoder.PushDebugGroup(pRenderJob->mName.c_str());

                // bind broup, pipeline, index buffer, vertex buffer, scissor rect, viewport, and draw
                for(uint32_t iGroup = 0; iGroup < (uint32_t)pRenderJob->maBindGroups.size(); iGroup++)
                {
                    renderPassEncoder.SetBindGroup(
                        iGroup,
                        pRenderJob->maBindGroups[iGroup]);
                }

                renderPassEncoder.SetPipeline(pRenderJob->mRenderPipeline);
                renderPassEncoder.SetIndexBuffer(
                    maBuffers["train-index-buffer"],
                    wgpu::IndexFormat::Uint32
                );
                renderPassEncoder.SetVertexBuffer(
                    0,
                    maBuffers["train-vertex-buffer"]
                );
                renderPassEncoder.SetScissorRect(
                    0,
                    0,
                    mCreateDesc.miScreenWidth,
                    mCreateDesc.miScreenHeight);
                renderPassEncoder.SetViewport(
                    0,
                    0,
                    (float)mCreateDesc.miScreenWidth,
                    (float)mCreateDesc.miScreenHeight,
                    0.0f,
                    1.0f);
                
                if(pRenderJob->mPassType == Render::PassType::DrawMeshes)
                {
#if defined(__EMSCRIPTEN__) || !defined(_MSC_VER)
                    for(uint32_t iMesh = 0; iMesh < (uint32_t)maMeshTriangleRanges.size(); iMesh++)
                    {
                        if(maiVisibilityFlags[iMesh] >= 1)
                        {
                            uint32_t iNumIndices = maMeshTriangleRanges[iMesh].miEnd - maMeshTriangleRanges[iMesh].miStart;
                            uint32_t iIndexOffset = maMeshTriangleRanges[iMesh].miStart;
                            //renderPassEncoder.DrawIndexed(iNumIndices, 1, iIndexOffset, 0, 0);
                            renderPassEncoder.DrawIndexedIndirect(
                                maRenderJobs["Mesh Culling Compute"]->mOutputBufferAttachments["Draw Calls"],
                                iMesh * 5 * sizeof(uint32_t)
                            );
                        }
                        //else
                        //{
                        //    printf("mesh %d not visible\n", iMesh);
                        //}
                    }
#else
                    renderPassEncoder.MultiDrawIndexedIndirect(
                        maRenderJobs["Mesh Culling Compute"]->mOutputBufferAttachments["Draw Calls"],
                        0,
                        (uint32_t)maMeshTriangleRanges.size(), //65536 * 2,
                        maRenderJobs["Mesh Culling Compute"]->mOutputBufferAttachments["Num Draw Calls"],
                        0
                    );
#endif // __EMSCRIPTEN__
                }
                else if(pRenderJob->mPassType == Render::PassType::FullTriangle)
                {
                    renderPassEncoder.Draw(3);
                }

                renderPassEncoder.PopDebugGroup();
                renderPassEncoder.End();

            }
            else if(pRenderJob->mType == Render::JobType::Compute)
            {
                wgpu::ComputePassDescriptor computePassDesc = {};
                wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass(&computePassDesc);
                
                computePassEncoder.PushDebugGroup(pRenderJob->mName.c_str());

                // bind broup, pipeline, index buffer, vertex buffer, scissor rect, viewport, and draw
                for(uint32_t iGroup = 0; iGroup < (uint32_t)pRenderJob->maBindGroups.size(); iGroup++)
                {
                    computePassEncoder.SetBindGroup(
                        iGroup,
                        pRenderJob->maBindGroups[iGroup]);
                }
                computePassEncoder.SetPipeline(pRenderJob->mComputePipeline);
                computePassEncoder.DispatchWorkgroups(
                    pRenderJob->mDispatchSize.x,
                    pRenderJob->mDispatchSize.y,
                    pRenderJob->mDispatchSize.z);
                
                computePassEncoder.PopDebugGroup();
                computePassEncoder.End();
            }
            else if(pRenderJob->mType == Render::JobType::Copy)
            {
                commandEncoder.PushDebugGroup(pRenderJob->mName.c_str());
                for(auto const& keyValue : pRenderJob->mInputImageAttachments)
                {
#if defined(__EMSCRIPTEN__)
                    wgpu::ImageCopyTexture srcInfo = {};
                    srcInfo.texture = *keyValue.second;
                    srcInfo.aspect = wgpu::TextureAspect::All;
                    srcInfo.mipLevel = 0;
                    srcInfo.origin.x = 0;
                    srcInfo.origin.y = 0;
                    srcInfo.origin.z = 0;

                    wgpu::ImageCopyTexture dstInfo = {};
                    dstInfo.texture = pRenderJob->mOutputImageAttachments[keyValue.first];
                    dstInfo.aspect = wgpu::TextureAspect::All;
                    dstInfo.mipLevel = 0;
                    dstInfo.origin.x = 0;
                    dstInfo.origin.y = 0;
                    dstInfo.origin.z = 0;

#else 
                    wgpu::TexelCopyTextureInfo srcInfo = {};
                    srcInfo.texture = *keyValue.second;
                    srcInfo.aspect = wgpu::TextureAspect::All;
                    srcInfo.mipLevel = 0;
                    srcInfo.origin.x = 0;
                    srcInfo.origin.y = 0;
                    srcInfo.origin.z = 0;

                    wgpu::TexelCopyTextureInfo dstInfo = {};
                    dstInfo.texture = pRenderJob->mOutputImageAttachments[keyValue.first];
                    dstInfo.aspect = wgpu::TextureAspect::All;
                    dstInfo.mipLevel = 0;
                    dstInfo.origin.x = 0;
                    dstInfo.origin.y = 0;
                    dstInfo.origin.z = 0;
#endif // __EMSCRIPTEN__
                    
                    wgpu::Extent3D copySize = {};
                    copySize.depthOrArrayLayers = 1;
                    copySize.width = srcInfo.texture.GetWidth();
                    copySize.height = srcInfo.texture.GetHeight();
                    commandEncoder.CopyTextureToTexture(&srcInfo, &dstInfo, &copySize);
                }
                commandEncoder.PopDebugGroup();
            }

            wgpu::CommandBuffer commandBuffer = commandEncoder.Finish();
            aCommandBuffer.push_back(commandBuffer);

        }   // for all render jobs

        // get selection info from shader via read back buffer
        if(mbWaitingForMeshSelection)
        {
            wgpu::CommandEncoderDescriptor commandEncoderDesc = {};
            wgpu::CommandEncoder commandEncoder = mpDevice->CreateCommandEncoder(&commandEncoderDesc);

            commandEncoder.CopyBufferToBuffer(
                maRenderJobs[mCaptureImageJobName]->mUniformBuffers[mCaptureUniformBufferName],
                0,
                mOutputImageBuffer,
                0,
                64
            );

            wgpu::CommandBuffer commandBuffer = commandEncoder.Finish();
            aCommandBuffer.push_back(commandBuffer);

            printf("copy selection buffer\n");
            mbSelectedBufferCopied = true;
        }

        // submit all the job commands
        mpDevice->GetQueue().Submit(
            (uint32_t)aCommandBuffer.size(), 
            aCommandBuffer.data());

        ++miFrame;
    }

    /*
    **
    */
    void CRenderer::createRenderJobs(CreateDescriptor& desc)
    {
#if defined(__EMSCRIPTEN__)
        char* acFileContentBuffer = nullptr;
        Loader::loadFile(
            &acFileContentBuffer,
            "render-jobs/" + desc.mRenderJobPipelineFilePath,
            true
        );
#else 
        std::vector<char> acFileContentBuffer;
        Loader::loadFile(
            acFileContentBuffer,
            "render-jobs/" + desc.mRenderJobPipelineFilePath,
            true
        );
#endif //__EMSCRIPTEN__

        Render::CRenderJob::CreateInfo createInfo = {};
        createInfo.miScreenWidth = desc.miScreenWidth;
        createInfo.miScreenHeight = desc.miScreenHeight;
        createInfo.mpfnGetBuffer = [](uint32_t& iBufferSize, std::string const& bufferName, void* pUserData)
        {
            Render::CRenderer* pRenderer = (Render::CRenderer*)pUserData;
            assert(pRenderer->maBuffers.find(bufferName) != pRenderer->maBuffers.end());
            
            iBufferSize = pRenderer->maBufferSizes[bufferName];
            return pRenderer->maBuffers[bufferName];
        };
        createInfo.mpUserData = this;

        rapidjson::Document doc;
        {
#if defined(__EMSCRIPTEN__)
            doc.Parse(acFileContentBuffer);
            Loader::loadFileFree(acFileContentBuffer);
#else 
            doc.Parse(acFileContentBuffer.data());
#endif //__EMSCRIPTEN__
        }

        std::vector<std::string> aRenderJobNames;
        std::vector<std::string> aShaderModuleFilePath;

        auto const& jobs = doc["Jobs"].GetArray();
        for(auto const& job : jobs)
        {
            createInfo.mName = job["Name"].GetString();
            std::string jobType = job["Type"].GetString();
            createInfo.mJobType = Render::JobType::Graphics;
            if(jobType == "Compute")
            {
                createInfo.mJobType = Render::JobType::Compute;
            }
            else if(jobType == "Copy")
            {
                createInfo.mJobType = Render::JobType::Copy;
            }

            maOrderedRenderJobs.push_back(createInfo.mName);

            std::string passStr = job["PassType"].GetString();
            if(passStr == "Compute")
            {
                createInfo.mPassType = Render::PassType::Compute;
            }
            else if(passStr == "Draw Meshes")
            {
                createInfo.mPassType = Render::PassType::DrawMeshes;
            }
            else if(passStr == "Full Triangle")
            {
                createInfo.mPassType = Render::PassType::FullTriangle;
            }
            else if(passStr == "Copy")
            {
                createInfo.mPassType = Render::PassType::Copy;
            }
            else if(passStr == "Swap Chain")
            {
                createInfo.mPassType = Render::PassType::SwapChain;
            }
            else if(passStr == "Depth Prepass")
            {
                createInfo.mPassType = Render::PassType::DepthPrepass;
            }
            else if(passStr == "Copy")
            {
                createInfo.mPassType = Render::PassType::Copy;
            }

            createInfo.mpDevice = mpDevice;

            std::string pipelineFilePath = std::string("render-jobs/") + job["Pipeline"].GetString();
            createInfo.mPipelineFilePath = pipelineFilePath;

            createInfo.mpSampler = desc.mpSampler;
            createInfo.mpTotalDiffuseTextureView = &mDiffuseTextureAtlasView;

            aShaderModuleFilePath.push_back(pipelineFilePath);

            maRenderJobs[createInfo.mName] = std::make_unique<Render::CRenderJob>();
            maRenderJobs[createInfo.mName]->createWithOnlyOutputAttachments(createInfo);

            if(jobType == "Compute")
            {
                if(job.HasMember("Dispatch"))
                {
                    auto dispatchArray = job["Dispatch"].GetArray();
                    maRenderJobs[createInfo.mName]->mDispatchSize.x = dispatchArray[0].GetUint();
                    maRenderJobs[createInfo.mName]->mDispatchSize.y = dispatchArray[1].GetUint();
                    maRenderJobs[createInfo.mName]->mDispatchSize.z = dispatchArray[2].GetUint();
                }
            }

            aRenderJobNames.push_back(createInfo.mName);
        }

        std::vector<Render::CRenderJob*> apRenderJobs;
        for(auto const& renderJobName : aRenderJobNames)
        {
            apRenderJobs.push_back(maRenderJobs[renderJobName].get());
        }

        createInfo.mpDrawTextOutputAttachment = &mFontOutputAttachment;
        createInfo.mpDefaultUniformBuffer = &maBuffers["default-uniform-buffer"];
        createInfo.mpaRenderJobs = &apRenderJobs;
        uint32_t iIndex = 0;
        for(auto const& renderJobName : aRenderJobNames)
        {
            createInfo.mName = renderJobName;
            createInfo.mJobType = maRenderJobs[renderJobName]->mType;
            createInfo.mPassType = maRenderJobs[renderJobName]->mPassType;
            createInfo.mPipelineFilePath = aShaderModuleFilePath[iIndex];
            
            if(maRenderJobs[renderJobName]->mType == Render::JobType::Copy)
            {
                maRenderJobs[renderJobName]->setCopyAttachments(createInfo);
            }
            else
            {
                maRenderJobs[renderJobName]->createWithInputAttachmentsAndPipeline(createInfo);
            }
            ++iIndex;
        }

    }

    /*
    **
    */
    wgpu::Texture& CRenderer::getSwapChainTexture()
    {
        //wgpu::Texture& swapChainTexture = maRenderJobs["Outline Graphics"]->mOutputImageAttachments["Line Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Ambient Occlusion Graphics"]->mOutputImageAttachments["Indirect Lighting Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Deferred Indirect Graphics"]->mOutputImageAttachments["Material Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["PBR Graphics"]->mOutputImageAttachments["PBR Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Composite Graphics"]->mOutputImageAttachments["Composite Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Ambient Occlusion Graphics"]->mOutputImageAttachments["Ambient Occlusion Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["TAA Graphics"]->mOutputImageAttachments["TAA Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Mesh Selection Graphics"]->mOutputImageAttachments["Selection Output"];
        wgpu::Texture& swapChainTexture = maRenderJobs["Final Composite Graphics"]->mOutputImageAttachments["Final Composite Output"];
        //assert(maRenderJobs.find("Mesh Selection Graphics") != maRenderJobs.end());
        //assert(maRenderJobs["Mesh Selection Graphics"]->mOutputImageAttachments.find("Selection Output") != maRenderJobs["Mesh Selection Graphics"]->mOutputImageAttachments.end());

        return swapChainTexture;
    }

    /*
    **
    */
    bool CRenderer::setBufferData(
        std::string const& jobName,
        std::string const& bufferName,
        void* pData,
        uint32_t iOffset,
        uint32_t iDataSize)
    {
        bool bRet = false;

        assert(maRenderJobs.find(jobName) != maRenderJobs.end());
        Render::CRenderJob* pRenderJob = maRenderJobs[jobName].get();
        if(pRenderJob->mUniformBuffers.find(bufferName) != pRenderJob->mUniformBuffers.end())
        {
            wgpu::Buffer uniformBuffer = pRenderJob->mUniformBuffers[bufferName];
            mpDevice->GetQueue().WriteBuffer(
                uniformBuffer,
                iOffset,
                pData,
                iDataSize
            );

            bRet = true;
        }

        return bRet;
    }

    /*
    **
    */
    bool CRenderer::setBufferData(
        std::string const& bufferName,
        void* pData,
        uint32_t iOffset,
        uint32_t iDataSize)
    {
        bool bRet = true;

        assert(maBuffers.find(bufferName) != maBuffers.end());
        mpDevice->GetQueue().WriteBuffer(
            maBuffers[bufferName],
            iOffset,
            pData,
            iDataSize
        );

        return bRet;
    }

    /*
    **
    */
    void CRenderer::highLightSelectedMesh(int32_t iX, int32_t iY)
    {
        mCaptureImageName = "Selection Output";
        mCaptureImageJobName = "Mesh Selection Graphics";
        mCaptureUniformBufferName = "selectedMesh";

        mSelectedCoord = int2(iX, iY);
        mSelectMeshInfo.miMeshID = 0;
    }

    /*
    **
    */
    CRenderer::SelectMeshInfo const& CRenderer::getSelectionInfo()
    {
        return mSelectMeshInfo;
    }

    /*
    **
    */
    void CRenderer::setupFontPipeline()
    {
        // output texture for draw text
        wgpu::TextureFormat aViewFormats[] = {wgpu::TextureFormat::RGBA8Unorm};
        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment;
        textureDesc.dimension = wgpu::TextureDimension::e2D;
        textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
        textureDesc.mipLevelCount = 1;
        textureDesc.sampleCount = 1;
        textureDesc.size.depthOrArrayLayers = 1;
        textureDesc.size.width = mCreateDesc.miScreenWidth;
        textureDesc.size.height = mCreateDesc.miScreenHeight;
        textureDesc.viewFormatCount = 1;
        textureDesc.viewFormats = aViewFormats;
        mFontOutputAttachment = mpDevice->CreateTexture(&textureDesc);

        wgpu::SamplerDescriptor samplerDesc = {};
        samplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
        samplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
        samplerDesc.minFilter = wgpu::FilterMode::Linear;
        samplerDesc.magFilter = wgpu::FilterMode::Linear;
        samplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
        mFontSampler = mpDevice->CreateSampler(&samplerDesc);
        mFontSampler.SetLabel("Font Sampler");

        // binding layouts
        std::vector<wgpu::BindGroupLayoutEntry> aBindingLayouts;
        wgpu::BindGroupLayoutEntry textureLayout = {};

        // texture binding layout
        // font atlas texture
        textureLayout.binding = (uint32_t)aBindingLayouts.size();
        textureLayout.visibility = (wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
        textureLayout.texture.sampleType = wgpu::TextureSampleType::Float;
        textureLayout.texture.viewDimension = wgpu::TextureViewDimension::e2D;
        aBindingLayouts.push_back(textureLayout);

        // glyph info
        wgpu::BindGroupLayoutEntry bufferLayout = {};
        bufferLayout.binding = (uint32_t)aBindingLayouts.size();
        bufferLayout.visibility = (wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
        bufferLayout.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        bufferLayout.buffer.minBindingSize = 0;
        aBindingLayouts.push_back(bufferLayout);

        // draw coordinate info
        bufferLayout.binding = (uint32_t)aBindingLayouts.size();
        bufferLayout.visibility = (wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
        bufferLayout.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        bufferLayout.buffer.minBindingSize = 0;
        aBindingLayouts.push_back(bufferLayout);

        // uniform buffer
        bufferLayout.binding = (uint32_t)aBindingLayouts.size();
        bufferLayout.visibility = (wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
        bufferLayout.buffer.type = wgpu::BufferBindingType::Uniform;
        bufferLayout.buffer.minBindingSize = 0;
        aBindingLayouts.push_back(bufferLayout);

        // default uniform buffer
        bufferLayout.binding = (uint32_t)aBindingLayouts.size();
        bufferLayout.visibility = (wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
        bufferLayout.buffer.type = wgpu::BufferBindingType::Uniform;
        bufferLayout.buffer.minBindingSize = 0;
        aBindingLayouts.push_back(bufferLayout);

        // sampler binding layout
        wgpu::BindGroupLayoutEntry samplerLayout = {};
        samplerLayout.binding = (uint32_t)aBindingLayouts.size();
        samplerLayout.sampler.type = wgpu::SamplerBindingType::Filtering;
        samplerLayout.visibility = wgpu::ShaderStage::Fragment;
        aBindingLayouts.push_back(samplerLayout);

        // create binding group layout
        wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc = {};
        bindGroupLayoutDesc.entries = aBindingLayouts.data();
        bindGroupLayoutDesc.entryCount = (uint32_t)aBindingLayouts.size();
        mFontBindGroupLayout = mpDevice->CreateBindGroupLayout(&bindGroupLayoutDesc);

        // create bind group
        std::vector<wgpu::BindGroupEntry> aBindGroupEntries;

        // texture binding in group
        // atlas texture
        wgpu::BindGroupEntry bindGroupEntry = {};
        bindGroupEntry.binding = (uint32_t)aBindGroupEntries.size();
        bindGroupEntry.textureView = maTextureViews["font-atlas-image"];
        bindGroupEntry.sampler = nullptr;
        aBindGroupEntries.push_back(bindGroupEntry);

        // font glyph info
        bindGroupEntry = {};
        bindGroupEntry.binding = (uint32_t)aBindGroupEntries.size();
        bindGroupEntry.buffer = maBuffers["font-info"];
        bindGroupEntry.sampler = nullptr;
        aBindGroupEntries.push_back(bindGroupEntry);

        // draw coordinate info
        bindGroupEntry = {};
        bindGroupEntry.binding = (uint32_t)aBindGroupEntries.size();
        bindGroupEntry.buffer = maBuffers["glyph-coordinates"];
        bindGroupEntry.sampler = nullptr;
        aBindGroupEntries.push_back(bindGroupEntry);

        // uniform buffer
        bindGroupEntry = {};
        bindGroupEntry.binding = (uint32_t)aBindGroupEntries.size();
        bindGroupEntry.buffer = maBuffers["draw-text-uniform"];
        bindGroupEntry.sampler = nullptr;
        aBindGroupEntries.push_back(bindGroupEntry);

        // default uniform 
        bindGroupEntry = {};
        bindGroupEntry.binding = (uint32_t)aBindGroupEntries.size();
        bindGroupEntry.buffer = maBuffers["default-uniform-buffer"];
        bindGroupEntry.sampler = nullptr;
        aBindGroupEntries.push_back(bindGroupEntry);

        // sample binding in group
        bindGroupEntry = {};
        bindGroupEntry.binding = (uint32_t)aBindGroupEntries.size();
        bindGroupEntry.sampler = mFontSampler;
        aBindGroupEntries.push_back(bindGroupEntry);

        // create bind group
        wgpu::BindGroupDescriptor bindGroupDesc = {};
        bindGroupDesc.layout = mFontBindGroupLayout;
        bindGroupDesc.entries = aBindGroupEntries.data();
        bindGroupDesc.entryCount = (uint32_t)aBindGroupEntries.size();
        mFontBindGroup = mpDevice->CreateBindGroup(&bindGroupDesc);
        mFontBindGroup.SetLabel("Draw Text Bind Group");

        // layout for creating pipeline
        wgpu::PipelineLayoutDescriptor layoutDesc = {};
        layoutDesc.bindGroupLayoutCount = 1;
        layoutDesc.bindGroupLayouts = &mFontBindGroupLayout;
        wgpu::PipelineLayout pipelineLayout = mpDevice->CreatePipelineLayout(&layoutDesc);
        pipelineLayout.SetLabel("Draw Text Pipeline Layout");

        wgpu::ShaderModuleWGSLDescriptor wgslDesc = {};
        std::string shaderPath = "shaders/draw_text.shader";
#if defined(__EMSCRIPTEN__)
        char* acShaderFileContent = nullptr;
        Loader::loadFile(
            &acShaderFileContent,
            shaderPath,
            true
        );
        wgslDesc.code = acShaderFileContent;

        //printf("shader content: %s\n", acShaderFileContent);

#else 
        std::vector<char> acShaderFileContent;
        Loader::loadFile(
            acShaderFileContent,
            shaderPath,
            true
        );
        wgslDesc.code = acShaderFileContent.data();
#endif // __EMSCRIPTEN__

        wgpu::ShaderModuleDescriptor shaderModuleDescriptor
        {
            .nextInChain = &wgslDesc
        };
        wgpu::ShaderModule shaderModule = mpDevice->CreateShaderModule(&shaderModuleDescriptor);
        wgpu::ColorTargetState colorTargetState
        {
            .format = wgpu::TextureFormat::RGBA8Unorm
        };

        wgpu::FragmentState fragmentState = {};
        wgpu::VertexState vertexState = {};
        wgpu::VertexBufferLayout vertexBufferLayout = {};
        std::vector<wgpu::VertexAttribute> aVertexAttributes;
        wgpu::VertexAttribute attrib = {};

        fragmentState.module = shaderModule;
        fragmentState.targetCount = 1;
        fragmentState.targets = &colorTargetState;
        fragmentState.entryPoint = "fs_main";

        attrib.format = wgpu::VertexFormat::Float32x4;
        attrib.offset = 0;
        attrib.shaderLocation = 0;
        aVertexAttributes.push_back(attrib);
        attrib.offset = sizeof(float4);
        attrib.shaderLocation = 1;
        aVertexAttributes.push_back(attrib);
        attrib.offset = sizeof(float4) * 2;
        attrib.shaderLocation = 2;
        aVertexAttributes.push_back(attrib);

        // vertex layout
        vertexBufferLayout.attributeCount = 3;
        vertexBufferLayout.arrayStride = sizeof(float4) * 3;
        vertexBufferLayout.attributes = aVertexAttributes.data();
        vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;

        // vertex shader
        vertexState.module = shaderModule;
        vertexState.entryPoint = "vs_main";
        vertexState.buffers = &vertexBufferLayout;
        vertexState.bufferCount = 1;
        
        // set the expected layout for pipeline and create
        wgpu::RenderPipelineDescriptor renderPipelineDesc = {};
        vertexState.module = shaderModule;
        renderPipelineDesc.vertex = vertexState;
        renderPipelineDesc.fragment = &fragmentState;
        renderPipelineDesc.layout = pipelineLayout;
        mDrawTextPipeline = mpDevice->CreateRenderPipeline(&renderPipelineDesc);
        mDrawTextPipeline.SetLabel("Draw Text Pipeline");

#if defined(__EMSCRIPTEN__)
        Loader::loadFileFree(acShaderFileContent);
#endif // __EMSCRIPTEN__

    }

    /*
    **
    */
    void CRenderer::drawText(
        std::vector<wgpu::CommandBuffer>& aCommandBuffers,
        std::string const& text, 
        uint32_t iX, 
        uint32_t iY, 
        uint32_t iSize,
        float3 const& color)
    {
        struct Coord
        {
            int32_t        miX;
            int32_t        miY;
            int32_t        miGlyphIndex;
        };

        float fGlyphScale = float(iSize) / 64.0f;

        uint32_t iBorderSize = 0;
        std::vector<Coord> aGlyphCoord;
        uint32_t iTextLength = (uint32_t)text.length();
        uint32_t iCurrX = iX, iCurrY = iY;
        for(uint32_t i = 0; i < iTextLength; i++)
        {
            int32_t iGlyphIndex = int32_t(text.at(i)) - 33;
            if(iGlyphIndex < 0)
            {
                iCurrX += 32;
                continue;
            }
            
            int32_t iGlyphWidth = int32_t(maFontInfo[iGlyphIndex].width * fGlyphScale);
            int32_t iGlyphHeight = int32_t(maFontInfo[iGlyphIndex].height * fGlyphScale);

            int32_t iGlyphX = iCurrX + iGlyphWidth / 2;
            int32_t iGlyphY = iCurrY + iGlyphHeight / 2;
        
            Coord coord = {iGlyphX, iGlyphY, iGlyphIndex};
            aGlyphCoord.push_back(coord);

            iCurrX += iGlyphWidth + iBorderSize;
        }

        mpDevice->GetQueue().WriteBuffer(
            maBuffers["glyph-coordinates"],
            0,
            aGlyphCoord.data(),
            aGlyphCoord.size() * sizeof(Coord)
        );

        struct UniformData
        {
            float4      mScale;
            float4      mColor;
        };
        UniformData uniformData;
        uniformData.mScale = float4(fGlyphScale, fGlyphScale, fGlyphScale, fGlyphScale);
        uniformData.mColor = float4(color.x, color.y, color.z, 1.0f);
        mpDevice->GetQueue().WriteBuffer(
            maBuffers["draw-text-uniform"],
            0,
            &uniformData,
            sizeof(UniformData)
        );

        wgpu::CommandEncoderDescriptor commandEncoderDesc = {};
        wgpu::CommandEncoder commandEncoder = mpDevice->CreateCommandEncoder(&commandEncoderDesc);

        wgpu::RenderPassColorAttachment attachment
        {
            .view = mFontOutputAttachment.CreateView(),
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store
        };
        wgpu::RenderPassDescriptor renderPassDesc = {};
        renderPassDesc.colorAttachmentCount = 1;
        renderPassDesc.colorAttachments = &attachment;
        renderPassDesc.depthStencilAttachment = nullptr;
        wgpu::RenderPassEncoder renderPassEncoder = commandEncoder.BeginRenderPass(&renderPassDesc);

        renderPassEncoder.PushDebugGroup("Draw Text");

        // bind broup, pipeline, index buffer, vertex buffer, scissor rect, viewport, and draw
        renderPassEncoder.SetBindGroup(
            0,
            mFontBindGroup);
        renderPassEncoder.SetPipeline(mDrawTextPipeline);
        renderPassEncoder.SetIndexBuffer(
            maBuffers["quad-index-buffer"],
            wgpu::IndexFormat::Uint32
        );
        renderPassEncoder.SetVertexBuffer(
            0,
            maBuffers["quad-vertex-buffer"]
        );
        renderPassEncoder.SetScissorRect(
            0,
            0,
            mCreateDesc.miScreenWidth,
            mCreateDesc.miScreenHeight);
        renderPassEncoder.SetViewport(
            0,
            0,
            (float)mCreateDesc.miScreenWidth,
            (float)mCreateDesc.miScreenHeight,
            0.0f,
            1.0f);
        
        renderPassEncoder.DrawIndexed(6, iTextLength, 0, 0, 0);
         
        renderPassEncoder.PopDebugGroup();
        renderPassEncoder.End();

        wgpu::CommandBuffer commandBuffer = commandEncoder.Finish();
        aCommandBuffers.push_back(commandBuffer);

    }

}   // Render