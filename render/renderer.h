#pragma once

#include <render/render_job.h>
#include <webgpu/webgpu_cpp.h>
#include <string>
#include <map>
#include <chrono>

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
            wgpu::Sampler* mpSampler;
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

        struct SelectMeshInfo
        {
            int32_t miMeshID = -1;
            int32_t miSelectionCoordX;
            int32_t miSelectionCoordY;
            int32_t miPadding;
            float4 mMinPosition;
            float4 mMaxPosition;
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

        bool setBufferData(
            std::string const& bufferName,
            void* pData,
            uint32_t iOffset,
            uint32_t iDataSize);

        void highLightSelectedMesh(int32_t iX, int32_t iY);

        SelectMeshInfo const& getSelectionInfo();

        inline uint32_t getNumMeshes()
        {
            return (uint32_t)maMeshTriangleRanges.size();
        }

        inline void setVisibilityFlags(uint32_t* piVisibilityFlags)
        {
            maiVisibilityFlags = piVisibilityFlags;
        }

        inline uint32_t getFrameIndex()
        {
            return miFrame;
        }

        inline void setCameraPositionAndLookAt(
            float3 const& cameraPosition,
            float3 const& cameraLookAt
        )
        {
            mCameraPosition = cameraPosition;
            mCameraLookAt = cameraLookAt;
        }

    public:
        struct MeshExtent
        {
            float4  mMinPosition;
            float4  mMaxPosition;
        };

        MeshExtent                              mTotalMeshExtent;

        struct QueueData
        {
            std::string         mJobName;
            std::string         mShaderResourceName;
            void* mpData;
            uint32_t            miStart;
            uint32_t            miSize;
        };

        std::vector<QueueData>                  maQueueData;

        inline void addQueueData(QueueData const& data)
        {
            maQueueData.push_back(data);
        }

        float  mfCrossSectionPlaneD = 1000000.0f;
        float3                                  mCameraPosition;
        float3                                  mCameraLookAt;

    protected:
        void createRenderJobs(CreateDescriptor& desc);

    protected:
        CreateDescriptor                        mCreateDesc;

        wgpu::Device* mpDevice;

        // TODO: move buffers output renderer
        std::map<std::string, wgpu::Buffer>     maBuffers;
        std::map<std::string, uint32_t>         maBufferSizes;
        std::map<std::string, std::unique_ptr<Render::CRenderJob>>   maRenderJobs;
        std::vector<std::string> maOrderedRenderJobs;

        uint32_t                                miFrame = 0;

        struct MeshTriangleRange
        {
            uint32_t miStart;
            uint32_t miEnd;
        };
        

        std::vector<MeshTriangleRange>          maMeshTriangleRanges;
        std::vector<MeshExtent>                 maMeshExtents;

        wgpu::Instance*                         mpInstance;

        wgpu::Sampler*                          mpSampler;

        

    protected:
        std::string                             mCaptureImageName = "";
        std::string                             mCaptureImageJobName = "";
        std::string                             mCaptureUniformBufferName = "";
        int2                                    mSelectedCoord = int2(-1, -1);
        wgpu::Buffer                            mOutputImageBuffer;
        
        
        SelectMeshInfo                          mSelectMeshInfo;
        
        bool                                    mbWaitingForMeshSelection = false;
        bool                                    mbUpdateUniform = false;
    
        uint32_t                                miStartCaptureFrame = 0;
        bool                                    mbSelectedBufferCopied = false;


        uint32_t*                               maiVisibilityFlags = nullptr;

        wgpu::Texture                           mDiffuseTextureAtlas;
        wgpu::TextureView                       mDiffuseTextureAtlasView;

        std::map<std::string, wgpu::Texture>              maTextures;
        std::map<std::string, wgpu::TextureView>          maTextureViews;

    protected:
        struct OutputGlyphInfo
        {
            int32_t                width;
            int32_t                height;
            int32_t                yOffset;
            int32_t                miAtlasX;
            int32_t                miAtlasY;
            int32_t                miASCII;
        };


        std::vector<OutputGlyphInfo>    maFontInfo;

        wgpu::BindGroupLayout   mFontBindGroupLayout;
        wgpu::BindGroup         mFontBindGroup;
        wgpu::Sampler           mFontSampler;
        wgpu::RenderPipeline    mDrawTextPipeline;

        wgpu::Texture           mFontOutputAttachment;

        void setupFontPipeline();
        void drawText(
            std::vector<wgpu::CommandBuffer>& aCommandBuffers,
            std::string const& text,
            uint32_t iX,
            uint32_t iY,
            uint32_t iSize,
            float3 const& color);

        std::string     mFPSOutput;
        std::chrono::high_resolution_clock::time_point mLastTimeStart;

    protected:
        std::string mSwapChainRenderJobName;
        std::string mSwapChainAttachmentName;

    public:
        inline void setSwapChainOutput(
            char const* szRenderJobName,
            char const* szOutputAttachmentName)
        {
            mSwapChainRenderJobName = szRenderJobName;
            mSwapChainAttachmentName = szOutputAttachmentName;
        }
    };

}   // Render