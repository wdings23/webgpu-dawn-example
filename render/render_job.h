#pragma once

#include <webgpu/webgpu_cpp.h>
#include <math/vec.h>
#include <render/render_utils.h>

#include <map>
#include <string>


namespace Render
{
	class CRenderJob
	{
	public:
		friend class CRenderer;
	public:
		struct CreateInfo
		{
			std::string											mName;
			wgpu::Device*										mpDevice;

			uint32_t                                            miScreenWidth;
			uint32_t                                            miScreenHeight;

			std::vector<CRenderJob*>*							mpaRenderJobs;

			wgpu::Buffer* mpDefaultUniformBuffer;
			uint3												mDispatchSize;

			wgpu::SurfaceTexture* mpSwapChain;

			std::string											mPipelineFilePath;
			Render::JobType											mJobType;
			Render::PassType										mPassType;

			wgpu::Sampler*											mpSampler;
		};
	public:
		CRenderJob() = default;
		virtual ~CRenderJob() = default;

		void createWithOnlyOutputAttachments(CreateInfo& createInfo);
		void createWithInputAttachmentsAndPipeline(CreateInfo& createInfo);

	protected:
		wgpu::SurfaceTexture* mpSwapChain;
		uint3													mDispatchSize = uint3(1, 1, 1);

		std::map<std::string, wgpu::Texture>					mOutputImageAttachments;
		std::map<std::string, wgpu::Texture*>					mInputImageAttachments;
		std::map<std::string, wgpu::Buffer>						mOutputBufferAttachments;
		std::map<std::string, wgpu::Buffer*>					mInputBufferAttachments;
		std::map<std::string, wgpu::Buffer>						mUniformBuffers;
		std::map<std::string, wgpu::Texture>					mUniformTextures;
		wgpu::Texture											mDepthStencilTexture;

		std::vector<wgpu::TextureFormat>						mOutputImageFormats;
		wgpu::TextureFormat										mDepthStencilViewFormat;

		std::vector<std::pair<std::string, std::pair<std::string, std::string>>>		mAttachmentOrder;
		std::vector<std::pair<std::string, std::pair<std::string, std::string>>>		mUniformOrder;

		std::vector<wgpu::RenderPassColorAttachment>									maOutputAttachments;

		std::string												mName;
		Render::JobType											mType;
		Render::PassType										mPassType;

		wgpu::RenderPipeline									mRenderPipeline;
		wgpu::RenderPassDescriptor								mRenderPassDesc;

		wgpu::ComputePipeline									mComputePipeline;

		wgpu::FrontFace											mFrontFace = wgpu::FrontFace::CCW;
		wgpu::CullMode											mCullMode = wgpu::CullMode::Back;
		
		wgpu::DepthStencilState									mDepthStencilState;
		
		std::vector<wgpu::BindGroup>							maBindGroups;
		
		wgpu::RenderPassDepthStencilAttachment					mDepthStencilAttachment;

		wgpu::LoadOp											mLoadOp = wgpu::LoadOp::Clear;
		wgpu::StoreOp											mStoreOp = wgpu::StoreOp::Store;
	};

}	// Render