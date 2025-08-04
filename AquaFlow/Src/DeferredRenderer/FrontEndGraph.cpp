#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderer/FrontEndGraph.h"
#include "Execution/GraphBuilder.h"
#include "DeferredRenderer/RenderGraph/ShadowPlugin.h"
#include "../Utils/CompilerErrorChecker.h"

AQUA_BEGIN

struct FrontEndGraphConfig
{
	RendererFeatureFlags mFeatures;
	ShadowCascadeFeature mShadowFeature;

	std::vector<std::string> mOutputs;

	FeatureInfoMap mFeatureInfos;
	vkLib::Buffer<CameraInfo> mCamera;

	EnvironmentRef mEnv;
	Mat4Buf mModels;

	std::vector<vkLib::Framebuffer> mDepthBuffers;
	std::vector<vkLib::ImageView> mDepthViews;

	VertexFactory* mVertexFactory = nullptr;
	RenderTargetFactory mFramebufferFactory;

	EXEC_NAMESPACE::Graph mGraph; // motion blur
	EXEC_NAMESPACE::GraphBuilder mGraphBuilder;
	EXEC_NAMESPACE::GraphList mGraphList;

	vkLib::Context mCtx;

	std::string mShaderDirectory = "D:\\Dev\\AquaFlow\\AquaFlow\\Assets\\Shaders\\Deferred\\";
	vkLib::PShader mDepthShader;
};

AQUA_END

AQUA_NAMESPACE::FrontEndGraph::FrontEndGraph()
{
	mConfig = std::make_shared<FrontEndGraphConfig>();

	mConfig->mDepthShader.SetFilepath("eVertex", mConfig->mShaderDirectory + "Shadow.vert");
	mConfig->mDepthShader.SetFilepath("eFragment", mConfig->mShaderDirectory + "Shadow.frag");

	auto errors = mConfig->mDepthShader.CompileShaders();

	CompileErrorChecker checker(mConfig->mShaderDirectory + "../Logging/ShaderError.glsl");
	checker.AssertOnError(errors);
}

void AQUA_NAMESPACE::FrontEndGraph::SetCtx(vkLib::Context ctx)
{
	mConfig->mCtx = ctx;
	mConfig->mFramebufferFactory.SetContextBuilder(ctx.FetchRenderContextBuilder(vk::PipelineBindPoint::eGraphics));
}

void AQUA_NAMESPACE::FrontEndGraph::SetEnvironment(EnvironmentRef env)
{
	mConfig->mEnv = env;
}

void AQUA_NAMESPACE::FrontEndGraph::SetFeaturesInfos(const FeatureInfoMap& featureInfo)
{
	mConfig->mFeatureInfos = featureInfo;
}

void AQUA_NAMESPACE::FrontEndGraph::SetFeatureFlags(RendererFeatureFlags flags)
{
	mConfig->mFeatures = flags;
}

void AQUA_NAMESPACE::FrontEndGraph::SetShadowFeature(const ShadowCascadeFeature& feature)
{
	mConfig->mShadowFeature = feature;
}

void AQUA_NAMESPACE::FrontEndGraph::SetVertexFactory(VertexFactory& factory)
{
	mConfig->mVertexFactory = &factory;
}

void AQUA_NAMESPACE::FrontEndGraph::PrepareFeatures()
{
	mConfig->mGraphBuilder.Clear();
	mConfig->mOutputs.clear();

	PrepareDepthCascades();
	CreateGraph();
}

void AQUA_NAMESPACE::FrontEndGraph::PrepareDepthCascades()
{
	if (mConfig->mFeatures && RenderingFeature::eShadow == RendererFeatureFlags(0))
		return;

	auto config = mConfig;

	ShadowPlugin plugin{};
	VertexBindingMap vertexBindings{};
	vertexBindings[0].AddAttribute(0, "RGB32F");
	vertexBindings[0].SetName(ENTRY_POSITION);
	vertexBindings[1].AddAttribute(1, "RGB32F");
	vertexBindings[1].SetName(ENTRY_METADATA);

	plugin.SetBindings(vertexBindings);
	plugin.SetShader(mConfig->mDepthShader);
	plugin.SetPipelineBuilder(mConfig->mCtx.MakePipelineBuilder());

	std::string shadowStage = "ShadowStage_";

	for (uint32_t i = 0; i < static_cast<uint32_t>(mConfig->mEnv->GetDirLightCount()); i++)
	{
		std::string nodeName = shadowStage + std::to_string(i);

		plugin.SetDepthBuffer(mConfig->mDepthBuffers[0]);
		plugin.SetCameraOffset(i);
		plugin.AddPlugin(mConfig->mGraphBuilder, nodeName);

		mConfig->mGraphBuilder[nodeName].UpdateFn = [config](EXEC_NAMESPACE::Operation& op)
			{
				auto& pipeline = *reinterpret_cast<ShadowPipeline*>(GetRefAddr(op.GFX));

				pipeline.SetClearDepthStencilValues(1.0f, 0);

				pipeline.SetVertexBuffer(0, (*config->mVertexFactory)[ENTRY_POSITION]);
				pipeline.SetVertexBuffer(1, (*config->mVertexFactory)[ENTRY_METADATA]);

				pipeline.SetIndexBuffer(config->mVertexFactory->GetIndexBuffer());

				pipeline.SetCamerasInfos(config->mEnv->GetLightBuffers().mDirCameraInfos);
				pipeline.SetModels(config->mModels);

				pipeline.UpdateDescriptors();
			};

		mConfig->mOutputs.emplace_back(nodeName);
	}
}

void AQUA_NAMESPACE::FrontEndGraph::CreateGraph()
{
	// all m by n cascade network are both the inputs and outputs
	mConfig->mGraph = *mConfig->mGraphBuilder.GenerateExecutionGraph(mConfig->mOutputs);
	mConfig->mGraphList = mConfig->mGraph.SortEntries();
}

void AQUA_NAMESPACE::FrontEndGraph::SetModels(Mat4Buf models)
{
	mConfig->mModels = models;
}

void AQUA_NAMESPACE::FrontEndGraph::UpdateGraph()
{
	mConfig->mGraph.Update();
}

void AQUA_NAMESPACE::FrontEndGraph::PrepareFramebuffers(const glm::uvec2& rendererResolution)
{
	PrepareDepthBuffers();
}

void AQUA_NAMESPACE::FrontEndGraph::PrepareDepthBuffers()
{
	mConfig->mFramebufferFactory.Clear();
	mConfig->mFramebufferFactory.SetDepthAttribute("Depth", "D24UN_S8U");
	mConfig->mFramebufferFactory.SetDepthProperties(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled);
	mConfig->mFramebufferFactory.SetTargetSize(mConfig->mShadowFeature.BaseResolution);

	auto error = mConfig->mFramebufferFactory.Validate();

	_STL_ASSERT(error, "Failed to validate depth factory");

	mConfig->mDepthBuffers.reserve(mConfig->mEnv->GetDirLightCount());

	for (const auto& lightSrc : mConfig->mEnv->GetDirLightSrcList())
	{
		auto depthBuffer = *mConfig->mFramebufferFactory.CreateFramebuffer();

		vkLib::ImageViewCreateInfo viewInfo{};
		viewInfo.Type = vk::ImageViewType::e2D;
		viewInfo.Format = vk::Format::eD24UnormS8Uint;
		viewInfo.ComponentMaps = { vk::ComponentSwizzle::eR };
		viewInfo.Subresource.aspectMask = vk::ImageAspectFlagBits::eDepth;
		viewInfo.Subresource.baseArrayLayer = 0;
		viewInfo.Subresource.baseMipLevel = 0;
		viewInfo.Subresource.layerCount = 1;
		viewInfo.Subresource.levelCount = 1;

		auto depthView = depthBuffer.GetDepthStencilAttachment()->CreateImageView(viewInfo);

		mConfig->mDepthBuffers.emplace_back(depthBuffer);
		mConfig->mDepthViews.push_back(depthView);
	}
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Graph AQUA_NAMESPACE::FrontEndGraph::GetGraph() const
{
	return mConfig->mGraph;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::GraphList AQUA_NAMESPACE::FrontEndGraph::GetGraphList() const
{
	return mConfig->mGraphList;
}

std::vector<std::string> AQUA_NAMESPACE::FrontEndGraph::GetOutputs() const
{
	return mConfig->mOutputs;
}

std::vector<vkLib::ImageView> AQUA_NAMESPACE::FrontEndGraph::GetDepthViews() const
{
	return mConfig->mDepthViews;
}

std::vector <vkLib::Framebuffer> AQUA_NAMESPACE::FrontEndGraph::GetDepthbuffers() const
{
	return mConfig->mDepthBuffers;
}
