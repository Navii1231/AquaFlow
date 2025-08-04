#pragma once
#include "../Application/Application.h"

#include "DeferredRenderer/Pipelines/PipelineConfig.h"
#include "DeferredRenderer/Renderable/CopyIndices.h"

#include "DeferredRenderer/Renderer/Renderer.h"
#include "Material/MaterialBuilder.h"
#include "DeferredRenderer/Renderable/RenderTargetFactory.h"
#include "DeferredRenderer/Renderable/RenderableBuilder.h"

#include "RenderableManager.h"

#include "Utils/EditorCamera.h"
#include "Utils/STB_Image.h"

class DeferredRendererTester : public Application
{
public:
	DeferredRendererTester(const ApplicationCreateInfo& createInfo);
	~DeferredRendererTester();;

	virtual bool OnStart() override;
	virtual bool OnUpdate(float ElaspedTime) override;

private:
	AquaFlow::Renderer mRenderer;
	std::vector <AquaFlow::MaterialInstance> mMaterialInstances;

	AquaFlow::EnvironmentRef mEnv;

	AquaFlow::MaterialBuilder mMaterialSystem;
	AquaFlow::RenderTargetFactory mFramebufferFactory;

	// Buffers
	vkLib::Buffer<AquaFlow::PBRMaterial> mMaterials;
	vkLib::Buffer<AquaFlow::DirectionalLightSrc> mDirLightSrcs;

	// Resources
	vkLib::ResourcePool mResourcePool;
	vkLib::CommandBufferAllocator mCmdAlloc;

	vkLib::Core::Ref<vk::Sampler> mSpareSampler;

	// RenderTarget resources
	vkLib::ImageView mImageView;

	// Executions stuff
	vk::CommandBuffer mDisplayCommandBuffer;
	vkLib::Core::Executor mExec;

	// Display stuff
	AquaFlow::EditorCamera mEditorCamera;
	AquaFlow::CameraBuf mCameraBuf;
	AquaFlow::DepthCameraBuf mLightCameras;

	// Sync
	vkLib::Core::Ref<vk::Semaphore> mImageAcquired;
	vkLib::Core::Ref<vk::Semaphore> mDisplayImageSignal;

	// 3D Models
	RenderableManager mRenderableManager;

	AquaFlow::StbImage mImageLoader;
	vkLib::Image mEnvironmentTexture;

	glm::vec3 mShadowScale = { 25.0f, 25.0f, 75.0f };

	std::string mShaderDir = "D:\\Dev\\AquaFlow\\AquaFlow\\Assets\\Shaders\\Deferred\\";

private:
	void DisplayOnScreen(vkLib::Image image);

	void CreateResources();

	void UpdateCamera(float elaspedTime);
	void PrepareFramebufferFactory();
	void PrepareRenderer();

	void PrepareSyncStuff();
	void PrepareMaterials();
};
