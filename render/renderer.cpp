#include <render/renderer.h>

#include <curl/curl.h>

#include <rapidjson/document.h>
#include <math/vec.h>
#include <math/mat4.h>
#include <loader/loader.h>


#include <iostream>
#include <string>
#include <vector>

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

        std::vector<char> acTriangleBuffer;
        Loader::loadFile(acTriangleBuffer, desc.mMeshFilePath);

        uint32_t const* piData = (uint32_t const*)acTriangleBuffer.data();
        uint32_t iNumMeshes = *piData++;
        uint32_t iNumTotalVertices = *piData++;
        uint32_t iNumTotalTriangles = *piData++;
        uint32_t iVertexSize = *piData++;
        uint32_t iTriangleStartOffset = *piData++;

        maMeshTriangleRanges.resize(iNumMeshes);
        memcpy(maMeshTriangleRanges.data(), piData, sizeof(MeshTriangleRange) * iNumMeshes);
        piData += (2 * iNumMeshes);

        MeshExtent const* pMeshExtent = (MeshExtent const*)piData;
        maMeshExtents.resize(iNumMeshes);
        memcpy(maMeshExtents.data(), pMeshExtent, sizeof(MeshExtent) * iNumMeshes);
        pMeshExtent += iNumMeshes;

        std::vector<Vertex> aTotalMeshVertices(iNumTotalVertices);
        Vertex const* pVertices = (Vertex const*)pMeshExtent;
        memcpy(aTotalMeshVertices.data(), pVertices, iNumTotalVertices * sizeof(Vertex));
        pVertices += iNumTotalVertices;

        std::vector<uint32_t> aiTotalMeshTriangleIndices(iNumTotalTriangles * 3);
        piData = (uint32_t const*)pVertices;
        memcpy(aiTotalMeshTriangleIndices.data(), piData, iNumTotalTriangles * 3 * sizeof(uint32_t));

        wgpu::BufferDescriptor bufferDesc = {};

        bufferDesc.size = iNumTotalVertices * sizeof(Vertex);
        bufferDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        maBuffers["train-vertex-buffer"] = device.CreateBuffer(&bufferDesc);
        maBuffers["train-vertex-buffer"].SetLabel("Train Vertex Buffer");

        bufferDesc.size = aiTotalMeshTriangleIndices.size() * sizeof(uint32_t);
        bufferDesc.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        maBuffers["train-index-buffer"] = device.CreateBuffer(&bufferDesc);
        maBuffers["train-index-buffer"].SetLabel("Train Index Buffer");

        bufferDesc.size = iNumTotalVertices * sizeof(Vertex);
        bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        maBuffers["train-mesh-triangle-ranges"] = device.CreateBuffer(&bufferDesc);
        maBuffers["train-mesh-triangle-ranges"].SetLabel("Train Mesh Triangle Ranges");

        bufferDesc.size = iNumTotalVertices * sizeof(MeshExtent);
        bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        maBuffers["train-mesh-extents"] = device.CreateBuffer(&bufferDesc);
        maBuffers["train-mesh-extents"].SetLabel("Train Mesh Extents");

        device.GetQueue().WriteBuffer(maBuffers["train-vertex-buffer"], 0, aTotalMeshVertices.data(), iNumTotalVertices * sizeof(Vertex));
        device.GetQueue().WriteBuffer(maBuffers["train-index-buffer"], 0, aiTotalMeshTriangleIndices.data(), aiTotalMeshTriangleIndices.size() * sizeof(uint32_t));
        device.GetQueue().WriteBuffer(maBuffers["train-mesh-triangle-ranges"], 0, maMeshTriangleRanges.data(), maMeshTriangleRanges.size() * sizeof(MeshTriangleRange));
        device.GetQueue().WriteBuffer(maBuffers["train-mesh-extents"], 0, maMeshExtents.data(), maMeshExtents.size() * sizeof(MeshExtent));

        bufferDesc.size = sizeof(DefaultUniformData);
        bufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        maBuffers["default-uniform-buffer"] = device.CreateBuffer(&bufferDesc);
        maBuffers["default-uniform-buffer"].SetLabel("Default Uniform Buffer");

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
        device.GetQueue().WriteBuffer(
            maBuffers["full-screen-triangle"], 
            0, 
            aFullScreenTriangles, 
            3 * sizeof(Vertex));

        createRenderJobs(desc);

        device.GetQueue().WriteBuffer(
            maRenderJobs["Mesh Culling Compute"]->mUniformBuffers["aMeshTriangleIndexRanges"],
            0,
            maMeshTriangleRanges.data(),
            maMeshTriangleRanges.size() * sizeof(MeshTriangleRange));

        device.GetQueue().WriteBuffer(
            maRenderJobs["Mesh Culling Compute"]->mUniformBuffers["aMeshBBox"],
            0,
            maMeshExtents.data(),
            maMeshExtents.size() * sizeof(MeshExtent));

        uint32_t aiUniformData[] = {(uint32_t)maMeshExtents.size(), 0, 0, 0};
        device.GetQueue().WriteBuffer(
            maRenderJobs["Mesh Culling Compute"]->mUniformBuffers["uniformBuffer"],
            0,
            aiUniformData,
            4 * sizeof(uint32_t));
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
                    //for(uint32_t iMesh = 0; iMesh < (uint32_t)maMeshTriangleRanges.size(); iMesh++)
                    //{
                    //    uint32_t iNumIndices = maMeshTriangleRanges[iMesh].miEnd - maMeshTriangleRanges[iMesh].miStart;
                    //    uint32_t iIndexOffset = maMeshTriangleRanges[iMesh].miStart;
                    //    //renderPassEncoder.DrawIndexed(iNumIndices, 1, iIndexOffset, 0, 0);
                    //    renderPassEncoder.DrawIndexedIndirect(
                    //        maRenderJobs["Mesh Culling Compute"]->mOutputBufferAttachments["Draw Calls"],
                    //        iMesh * 5 * sizeof(uint32_t)
                    //    );
                    //}

                    renderPassEncoder.MultiDrawIndexedIndirect(
                        maRenderJobs["Mesh Culling Compute"]->mOutputBufferAttachments["Draw Calls"],
                        0,
                        65536,
                        maRenderJobs["Mesh Culling Compute"]->mOutputBufferAttachments["Num Draw Calls"],
                        0
                    );
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

            wgpu::CommandBuffer commandBuffer = commandEncoder.Finish();
            aCommandBuffer.push_back(commandBuffer);
        }

        // submit all the job commands
        mpDevice->GetQueue().Submit(
            (uint32_t)aCommandBuffer.size(), 
            aCommandBuffer.data());

#if 0
        // wait until done
        static bool sbWorkDone;
        sbWorkDone = false;
        mpDevice->GetQueue().OnSubmittedWorkDone(
            [](WGPUQueueWorkDoneStatus status, void* userdata)
            {
                sbWorkDone = true;
            },
            nullptr
        );

        while(sbWorkDone == false);
#endif // #if 0

        ++miFrame;
    }

    /*
    **
    */
    void CRenderer::createRenderJobs(CreateDescriptor& desc)
    {
        std::vector<char> acFileContentBuffer;
        Loader::loadFile(
            acFileContentBuffer,
            "render-jobs/" + desc.mRenderJobPipelineFilePath,
            true
        );

        Render::CRenderJob::CreateInfo createInfo = {};
        createInfo.miScreenWidth = 512;
        createInfo.miScreenHeight = 512;
        rapidjson::Document doc;
        {
            doc.Parse(acFileContentBuffer.data());
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

            createInfo.mpDevice = mpDevice;

            std::string pipelineFilePath = std::string("render-jobs/") + job["Pipeline"].GetString();
            createInfo.mPipelineFilePath = pipelineFilePath;

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
            maRenderJobs[renderJobName]->createWithInputAttachmentsAndPipeline(createInfo);
            ++iIndex;
        }

    }

    /*
    **
    */
    wgpu::Texture& CRenderer::getSwapChainTexture()
    {
        wgpu::Texture& swapChainTexture = maRenderJobs["Deferred Indirect Graphics"]->mOutputImageAttachments["Normal Output"];

        return swapChainTexture;
    }

}   // Render