#pragma once
#include "RendererConfig.h"
#include "Features.h"
#include "Environment.h"
#include "../Renderable/Renderable.h"
#include "../../Material/MaterialInstance.h"
#include "../../Execution/GraphBuilder.h"

#include "../Renderable/BasicRenderables.h"

AQUA_BEGIN

struct RendererConfig;

// It's going to work like a #state_machine as almost everything else here
class Renderer
{
public:
	Renderer();
	~Renderer();

	RendererFeatureFlags GetEnabledFeatures();

	void SetCtx(vkLib::Context ctx);

	void EnableFeatures(RendererFeatureFlags flags);
	void DisableFeatures(RendererFeatureFlags flags);

	void SetShadingbuffer(vkLib::Framebuffer framebuffer);

	void SetSSAOConfig(const SSAOFeature& config);
	void SetShadowConfig(const ShadowCascadeFeature& config);
	void SetBloomEffectConfig(const BloomEffectFeature& config);
	void SetEnvironment(EnvironmentRef env);
	void PrepareFeatures(); // first stage of preparation; setting up the renderer features and the environment

	void InsertPreEventDependency(vkLib::Core::Ref<vk::Semaphore> signal);
	void InsertPostEventDependency(vkLib::Core::Ref<vk::Semaphore> signal,
		vk::PipelineStageFlags pipelineFlags = vk::PipelineStageFlagBits::eTopOfPipe);

	// independent of the renderer state
	void SetCamera(const glm::mat4& projection, const glm::mat4& view);

	void SubmitRenderable(const std::string& name, const glm::mat4& model, Renderable renderable, MaterialInstance instance);
	void SubmitRenderable(const std::string& name, const glm::mat4& model, const MeshData& mesh, MaterialInstance instance);

	// Rendering different types of primitives
	// need a different kind of material to render lines
	// will help us in debugging and visualizing without setting up the renderables
	// it's a good idea to batch all line, curves and points together and submit them at once
	void SubmitLines(const std::string& lineIsland, const vk::ArrayProxy<Line>& lines);
	void SubmitCurves(const std::string& curveIsland, const vk::ArrayProxy<Curve>& connections);
	void SubmitBezierCurves(const std::string& curveIsland, const vk::ArrayProxy<Curve>& curves);
	// we could even render points in the three space
	void SubmitPoints(const std::string& pointIsland, const vk::ArrayProxy<Point>& points);

	void RemoveRenderable(const std::string& name);
	void ClearRenderables();

	void PrepareMaterialNetwork(); // passing the second stage

	// we're now in the active stage. We're free to select any renderable, upload memory to the GPU, and issue draw calls
	// any of the function below can be called at each frame without drastically impacting the performance
	SurfaceType GetSurfaceType(const std::string& name);

	void ActivateRenderables(const vk::ArrayProxy<std::string>& names);
	void DeactivateRenderables(const vk::ArrayProxy<std::string>& names);
	void UploadRenderables(); // uploading vertices to GPU; can be done each frame

	void UpdateDescriptors(); // updating the material and env descriptors

	void IssueDrawCall(); // we're free to issue the draw call here

	// getters, only active after prepare features function
	vkLib::Framebuffer GetPostprocessbuffer() const;
	vkLib::Framebuffer GetShadingbuffer() const;

	VertexBindingMap GetVertexBindings() const;

	// for debugging...
	vkLib::Framebuffer GetGBuffer() const;
	vkLib::Framebuffer GetDepthbuffer() const;
	vkLib::ImageView GetDepthView() const;

private:
	SharedRef<RendererConfig> mConfig;

private:
	void InsertModelMatrix(const glm::mat4& model);
	void InsertMaterial(MaterialInstance& instance, EXEC_NAMESPACE::OpFn&& opFn, EXEC_NAMESPACE::OpUpdateFn&& updateFn);
	void UpdateLineMaterial(const EXEC_NAMESPACE::Operation& op);

	void UploadModels();
	void UploadLines();
	void UploadPoints();

	void PrepareShadingNetwork();
	void PrepareFramebuffers();
	void ConnectFrontEndToShadingNetwork();
	void ConnectBackEndToShadingNetwork();

	void SetupFeatureInfos();
	void SetupVertexBindings();
	void SetupVertexFactory();

	void UpdateMaterialData();
	void ExecuteMaterial(vk::CommandBuffer buffer, const EXEC_NAMESPACE::Operation& op, uint32_t materialIdx);
	void ExecuteLineMaterial(const EXEC_NAMESPACE::Operation& op, vk::CommandBuffer buffer);

	void ResizeCmdBufferPool(size_t newSize);
	void SetupShaders();
	void ReserveVertexFactorySpace(uint32_t vertexCount, uint32_t indexCount);

	uint32_t CalculateActiveVertexCount();
	uint32_t CalculateActiveIndexCount();
	uint32_t CalculateActiveHSLVertexCount();
	uint32_t CalculateActiveHSPVertexCount();

	void SetupHSTargetCtx();
	void SetupHSVFactories();
	vkLib::GenericBuffer CreateHSVBuffer();
};

AQUA_END
