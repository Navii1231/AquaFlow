#pragma once
#include "../Application/Application.h"
#include "SampleGenerationTester.h"
#include "../PhotonFlux/ComputeEstimator.h"
#include "Wavefront/LocalRadixSortPipeline.h"
#include "Wavefront/SortRecorder.h"

#include "Wavefront/MergeSorterPipeline.h"

#include "Utils/EditorCamera.h"

#include "Wavefront/RayGenerationPipeline.h"
#include "Wavefront/WavefrontEstimator.h"

// Vulkan image loading stuff...
#include "Utils/STB_Image.h"

class ComputePipelineTester : public Application
{
public:
	ComputePipelineTester(const ApplicationCreateInfo& createInfo)
		: Application(createInfo) {}

	virtual bool OnStart() override;
	virtual bool OnUpdate(float ElaspedTime) override;

private:
	// Pipelines...
	vkLib::PipelineBuilder mPipelineBuilder;

	AquaFlow::PhFlux::LocalRadixSortPipeline mPrefixSummer;

	// Executor...
	vkLib::Core::Executor mComputeWorker;
	vkLib::Core::Executor mGraphicsWorker;

	// Resource allocators...
	vkLib::ResourcePool mResourcePool;

	vkLib::RenderContextBuilder mRenderContextBuilder;
	vkLib::RenderTargetContext mRenderContext;
	vkLib::Framebuffer mRenderTarget;

	PhFlux::CameraData mCameraData;

	AquaFlow::EditorCamera mVisualizerCamera;

	PhFlux::TraceResult mComputeTraceResult;
	AquaFlow::PhFlux::TraceResult mTraceResult;

	// Monte Carlo estimator...
	std::shared_ptr<PhFlux::ComputeEstimator> mEstimator;

	std::vector<glm::vec3> mRaySamples;

	std::shared_ptr<vkLib::Swapchain> mSwapchain;

	float mCurrTimeStep = 0.0f;

	AquaFlow::StbImage mImageLoader;

	// Wavefront path tracer

	AquaFlow::EditorCamera mEditorCamera;
	std::shared_ptr<AquaFlow::PhFlux::WavefrontEstimator> mWavefrontEstimator;

	AquaFlow::PhFlux::TraceSession mTraceSession;
	AquaFlow::PhFlux::Executor mExecutor;

	// For debugging...

	// Material pipelines...
	AquaFlow::MaterialInstance mDiffuseMaterial;
	AquaFlow::MaterialInstance mRefractionMaterial;
	AquaFlow::MaterialInstance mGlossyMaterial;
	AquaFlow::MaterialInstance mCookTorranceMaterial;
	AquaFlow::MaterialInstance mGlassMaterial;

	std::vector<AquaFlow::PhFlux::Ray> mHostRayBuffer;
	std::vector<AquaFlow::PhFlux::CollisionInfo> mHostCollisionInfoBuffer;
	std::vector<AquaFlow::PhFlux::RayRef> mHostRayRefBuffer;
	std::vector<AquaFlow::PhFlux::RayInfo> mHostRayInfoBuffer;
	std::vector<uint32_t> mHostRefCounts;

	bool UpdateCamera(AquaFlow::EditorCamera& camera);

private:
	void RayTrace(vk::CommandBuffer commandBuffer, vkLib::Framebuffer renderTarget);
	void RayTracerWithWavefront(vk::CommandBuffer commandBuffer, vkLib::Framebuffer renderTarget);


	void PresentScreen(uint32_t FrameIndex, vkLib::SwapchainFrame& ActiveFrame);

	void CheckErrors(const std::vector<vkLib::CompileError>& Errors);

	AquaFlow::CameraMovementFlags GetMovementFlags() const;

	void SetupCameras();

	// Scene setup...
	void SetScene();
	void SetSceneWavefront();

	void CreateMaterials();

	// For wavefront debugging
	void FillWavefrontHostBuffers();

	void PrintWavefrontRayHostBuffer();
	void PrintWavefrontCollisionInfoHostBuffer();
	void PrintWavefrontRayRefHostBuffer();
	void PrintWavefrontMaterialRefCountHostBuffer();
	void PrintWavefrontRayInfoHostBuffer();
	void TestNodeSystem();
};
