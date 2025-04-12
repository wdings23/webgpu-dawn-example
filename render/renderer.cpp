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

#include <utils/LogPrint.h>

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

        struct MeshSelectionUniformData
        {
            int32_t miSelectedMesh;
            int32_t miSelectionX;
            int32_t miSelectionY;
        };

        // changes need to be accounted for in uniform
        if(mbUpdateUniform)
        {
            struct UniformData
            {
                uint32_t miNumMeshes;
                float mfExplodeMultiplier;
            };

            UniformData uniformBuffer;
            uniformBuffer.miNumMeshes = (uint32_t)maMeshTriangleRanges.size();
            uniformBuffer.mfExplodeMultiplier = mfExplosionMult;

            mpDevice->GetQueue().WriteBuffer(
                maRenderJobs["Deferred Indirect Graphics"]->mUniformBuffers["indirectUniformData"],
                0,
                &uniformBuffer,
                sizeof(UniformData)
            );
        }

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

        // add commands from the render jobs
        std::vector<wgpu::CommandBuffer> aCommandBuffer;
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
#if defined(__EMSCRIPTEN__)
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
        wgpu::Texture& swapChainTexture = maRenderJobs["Composite Graphics"]->mOutputImageAttachments["Composite Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Ambient Occlusion Graphics"]->mOutputImageAttachments["Ambient Occlusion Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["TAA Graphics"]->mOutputImageAttachments["TAA Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Mesh Selection Graphics"]->mOutputImageAttachments["Selection Output"];
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

}   // Render