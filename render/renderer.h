#pragma once

#include <render/render_job.h>
#include <webgpu/webgpu_cpp.h>
#include <string>
#include <map>

#include <math/mat4.h>

namespace Render
{
    class CRenderer
    {
    public:
        struct CreateDescriptor
        {
            wgpu::Instance* mpInstance;
            wgpu::Device* mpDevice;
            uint32_t miScreenWidth;
            uint32_t miScreenHeight;
            std::string mMeshFilePath;
            std::string mRenderJobPipelineFilePath;
        };

        struct DrawUpdateDescriptor
        {
            float4x4 const* mpViewMatrix;
            float4x4 const* mpProjectionMatrix;
            float4x4 const* mpViewProjectionMatrix;
            float4x4 const* mpPrevViewProjectionMatrix;

            float3 const* mpCameraPosition;
            float3 const* mpCameraLookAt;
        };

    public:
        CRenderer() = default;
        virtual ~CRenderer() = default;

        void setup(CreateDescriptor& desc);
        void draw(DrawUpdateDescriptor& desc);

        wgpu::Texture& getSwapChainTexture();

        bool setBufferData(
            std::string const& jobName,
            std::string const& bufferName,
            void* pData,
            uint32_t iOffset,
            uint32_t iDataSize);

        void highLightSelectedMesh(int32_t iX, int32_t iY);

    protected:
        void createRenderJobs(CreateDescriptor& desc);

    protected:
        CreateDescriptor                        mCreateDesc;

        wgpu::Device* mpDevice;

        // TODO: move buffers output renderer
        std::map<std::string, wgpu::Buffer>     maBuffers;
        std::map<std::string, std::unique_ptr<Render::CRenderJob>>   maRenderJobs;
        std::vector<std::string> maOrderedRenderJobs;

        uint32_t                                miFrame = 0;

        struct MeshTriangleRange
        {
            uint32_t miStart;
            uint32_t miEnd;
        };
        struct MeshExtent
        {
            float4  mMinPosition;
            float4  mMaxPosition;
        };

        std::vector<MeshTriangleRange>          maMeshTriangleRanges;
        std::vector<MeshExtent>                 maMeshExtents;

        wgpu::Instance*                         mpInstance;

    protected:
        std::string                             mCaptureImageName = "";
        std::string                             mCaptureImageJobName = "";
        int2                                    mSelectedCoord;
        wgpu::Buffer                            mOutputImageBuffer;
    };
}   // Render