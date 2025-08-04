#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderer/BackEndGraph.h"
#include "DeferredRenderer/Renderer/Environment.h"
#include "Execution/GraphBuilder.h"
#include "DeferredRenderer/RenderGraph/PostProcessPlugin.h"
#include "../Utils/CompilerErrorChecker.h"

AQUA_BEGIN

struct BackEndGraphConfig
{
	RendererFeatureFlags mFeatures;

	std::vector<std::string> mInputs;

	FeatureInfoMap mFeatureInfos;
	vkLib::Buffer<CameraInfo> mCamera;

	EnvironmentRef mEnv;
	Mat4Buf mModels;

	vkLib::Framebuffer mShadingBuffer;
	vkLib::Framebuffer mPostProcessingBuffer;

	RenderTargetFactory mFramebufferFactory;

	vkLib::ResourcePool mResourcePool;

	// SSAO, Bloom effect, screen space reflections, post processing effects, skybox stuff
	EXEC_NAMESPACE::Graph mGraph; // motion blur
	EXEC_NAMESPACE::GraphBuilder mGraphBuilder;
	EXEC_NAMESPACE::GraphList mGraphList;

	vkLib::Core::Ref<vk::Sampler> mPostProcessSampler;

	vkLib::Context mCtx;

	// Shader stuff...
	std::string mShaderDirectory = "D:\\Dev\\AquaFlow\\AquaFlow\\Assets\\Shaders\\Deferred\\";
	vkLib::PShader mPostProcessingShader;
};

AQUA_END

AQUA_NAMESPACE::BackEndGraph::BackEndGraph()
{
	mConfig = std::make_shared<BackEndGraphConfig>();

	mConfig->mPostProcessingShader.SetFilepath("eVertex", mConfig->mShaderDirectory + "PostProcess.vert");
	mConfig->mPostProcessingShader.SetFilepath("eFragment", mConfig->mShaderDirectory + "PostProcess.frag");

	auto errors = mConfig->mPostProcessingShader.CompileShaders();

	CompileErrorChecker checker(mConfig->mShaderDirectory + "../Logging/ShaderError.glsl");
	checker.AssertOnError(errors);
}

void AQUA_NAMESPACE::BackEndGraph::SetCtx(vkLib::Context ctx)
{
	mConfig->mCtx = ctx;
	mConfig->mFramebufferFactory.SetContextBuilder(ctx.FetchRenderContextBuilder(vk::PipelineBindPoint::eGraphics));
	mConfig->mResourcePool = ctx.CreateResourcePool();

	mConfig->mPostProcessSampler = mConfig->mResourcePool.CreateSampler({});
}

void AQUA_NAMESPACE::BackEndGraph::SetEnvironment(EnvironmentRef env)
{
	mConfig->mEnv = env;
}

void AQUA_NAMESPACE::BackEndGraph::SetFeaturesInfos(const FeatureInfoMap& featureInfo)
{
	mConfig->mFeatureInfos = featureInfo;
}

void AQUA_NAMESPACE::BackEndGraph::SetFeatureFlags(RendererFeatureFlags flags)
{
	mConfig->mFeatures = flags;
}

void AQUA_NAMESPACE::BackEndGraph::PrepareFeatures()
{
	mConfig->mGraphBuilder.Clear();
	mConfig->mInputs.clear();

	PrepareSSAO();
	PrepareBloomEffect();
	PreparePostProcessing();

	CreateGraph();
}

void AQUA_NAMESPACE::BackEndGraph::PrepareSSAO()
{
	if (mConfig->mFeatures && RenderingFeature::eSSAO == RendererFeatureFlags(0))
		return;

}

void AQUA_NAMESPACE::BackEndGraph::PrepareBloomEffect()
{
	if (mConfig->mFeatures && RenderingFeature::eBloomEffect == RendererFeatureFlags(0))
		return;
}

void AQUA_NAMESPACE::BackEndGraph::PreparePostProcessing()
{
	auto config = mConfig;

	PostProcessPlugin plugin{};
	plugin.SetFramebuffer(mConfig->mPostProcessingBuffer);
	plugin.SetPipelineBuilder(mConfig->mCtx.MakePipelineBuilder());
	plugin.SetPShader(mConfig->mPostProcessingShader);
	plugin.AddPlugin(mConfig->mGraphBuilder, "PostProcess");

	mConfig->mGraphBuilder["PostProcess"].UpdateFn = [config](EXEC_NAMESPACE::Operation& op)
		{
			auto& pipeline = *reinterpret_cast<TextureVisualizer*>(GetRefAddr(op.GFX));

			pipeline.UpdateTexture(config->mShadingBuffer.GetColorAttachments().front(), config->mPostProcessSampler);
		};

	mConfig->mInputs.emplace_back("PostProcess");
}

void AQUA_NAMESPACE::BackEndGraph::CreateGraph()
{
	mConfig->mGraph = *mConfig->mGraphBuilder.GenerateExecutionGraph({ "PostProcess" });
	mConfig->mGraphList = mConfig->mGraph.SortEntries();
}

void AQUA_NAMESPACE::BackEndGraph::SetModels(Mat4Buf models)
{
	mConfig->mModels = models;
}

void AQUA_NAMESPACE::BackEndGraph::UpdateGraph()
{
	mConfig->mGraph.Update();
}

void AQUA_NAMESPACE::BackEndGraph::PrepareFramebuffers(const glm::uvec2& rendererResolution)
{
	auto& postProcessFac = mConfig->mFramebufferFactory;

	postProcessFac.AddColorAttribute("ColorOutput", "RGBA8UN");
	postProcessFac.SetAllColorProperties(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment);

	auto error = postProcessFac.Validate();

	_STL_ASSERT(error, "can't validate the post processing framebuffer");

	postProcessFac.SetTargetSize(rendererResolution);
	mConfig->mPostProcessingBuffer = *postProcessFac.CreateFramebuffer();
}

void AQUA_NAMESPACE::BackEndGraph::SetShadingFrameBuffer(vkLib::Framebuffer framebuffer)
{
	mConfig->mShadingBuffer = framebuffer;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Graph AQUA_NAMESPACE::BackEndGraph::GetGraph() const
{
	return mConfig->mGraph;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::GraphList AQUA_NAMESPACE::BackEndGraph::GetGraphList() const
{
	return mConfig->mGraphList;
}

std::vector<std::string> AQUA_NAMESPACE::BackEndGraph::GetInputs() const
{
	return mConfig->mInputs;
}

vkLib::Framebuffer AQUA_NAMESPACE::BackEndGraph::GetPostprocessbuffer() const
{
	return mConfig->mPostProcessingBuffer;
}
