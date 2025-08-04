#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderer/MaterialSystem.h"

#include "DeferredRenderer/Pipelines/DeferredPipeline.h"
#include "DeferredRenderer/Pipelines/HyperSurface.h"

#include "Material/MaterialInstance.h"
#include "Material/MaterialBuilder.h"

#include "DeferredRenderer/Renderable/RenderTargetFactory.h"

#include "Utils/CompilerErrorChecker.h"

AQUA_BEGIN

using MaterialCache = std::unordered_map<std::string, SharedRef<MaterialTemplate>>;

struct MaterialSystemConfig
{
	MaterialCache mCache;

	HyperSurfacePipeline mPointPipeline;
	HyperSurfacePipeline mLinePipeline;

	RenderTargetFactory mHyperSurfaceCtxFactory;
	vkLib::RenderContextBuilder mRenderCtxBuilder;

	MaterialBuilder mBuilder;
	vkLib::PipelineBuilder mPipelineBuilder;

	std::string mShaderDirectory = "D:\\Dev\\AquaFlow\\AquaFlow\\Assets\\Shaders\\Deferred\\";

	vkLib::PShader mHyperSurfaceShader;

	vkLib::Context mCtx;
};

AQUA_END

AQUA_NAMESPACE::MaterialSystem::MaterialSystem()
{
	mConfig = MakeRef<MaterialSystemConfig>();
}

void AQUA_NAMESPACE::MaterialSystem::SetShaderDirectory(const std::string& directory)
{
	mConfig->mShaderDirectory = directory;

	vkLib::PShader& shader = mConfig->mHyperSurfaceShader;
	shader.SetFilepath("eVertex", mConfig->mShaderDirectory + "HyperSurface.vert");
	shader.SetFilepath("eFragment", mConfig->mShaderDirectory + "HyperSurface.frag");

	auto errors = shader.CompileShaders();

	CompileErrorChecker errorChecker(mConfig->mShaderDirectory + "Logging/Error.glsl");
	errorChecker.GetErrors(errors);
	errorChecker.AssertOnError(errors);

	BuildSparePipelines();
}

void AQUA_NAMESPACE::MaterialSystem::SetHyperSurfaceRenderFactory(RenderTargetFactory factory)
{
	mConfig->mHyperSurfaceCtxFactory = factory;
	mConfig->mHyperSurfaceCtxFactory.SetContextBuilder(mConfig->mRenderCtxBuilder);

	_STL_VERIFY(mConfig->mHyperSurfaceCtxFactory.Validate(), "invalid render context");

	BuildSparePipelines();
}

void AQUA_NAMESPACE::MaterialSystem::SetCtx(vkLib::Context ctx)
{
	mConfig->mCtx = ctx;

	auto builder = ctx.MakePipelineBuilder();
	mConfig->mRenderCtxBuilder = ctx.FetchRenderContextBuilder(vk::PipelineBindPoint::eGraphics);
	mConfig->mPipelineBuilder = builder;

	MAT_NAMESPACE::MaterialAssembler assembler{};
	assembler.SetPipelineBuilder(builder);

	mConfig->mBuilder.SetAssembler(assembler);
	mConfig->mBuilder.SetResourcePool(mConfig->mCtx.CreateResourcePool());

	SetupDefaultRenderFactory();

	SetShaderDirectory(mConfig->mShaderDirectory);
}

AQUA_NAMESPACE::MaterialInstance AQUA_NAMESPACE::MaterialSystem::BuildInstance(
	const std::string& name, const DeferGFXMaterialCreateInfo& createInfo) const
{
	auto instance = mConfig->mBuilder.BuildDeferGFXInstance(createInfo);
	auto temp = CreateTemplate(name, createInfo, instance);

	mConfig->mCache[name] = temp;

	return instance;
}

void AQUA_NAMESPACE::MaterialSystem::SetupDefaultRenderFactory()
{
	auto& renderFac = mConfig->mHyperSurfaceCtxFactory;

	renderFac.SetContextBuilder(mConfig->mRenderCtxBuilder);
	renderFac.AddColorAttribute("Color", "RGBA32F");
	renderFac.SetDepthAttribute("Depth", "D24UN_S8U");

	renderFac.SetAllColorProperties(vk::AttachmentLoadOp::eLoad,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);

	renderFac.SetDepthProperties(vk::AttachmentLoadOp::eLoad,
		vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled);

	// preparing the render factory; it holds the render context
	_STL_VERIFY(renderFac.Validate(), "invalid render context");
}

void AQUA_NAMESPACE::MaterialSystem::InitInstance(MaterialTemplate& temp, MaterialInstance& instance) const
{
	mConfig->mBuilder.InitializeInstance(instance, temp.MatOp, temp.ParameterSet);
}

AQUA_NAMESPACE::SharedRef<AQUA_NAMESPACE::MaterialTemplate> AQUA_NAMESPACE::MaterialSystem::
	CreateTemplate(const std::string& name, const DeferGFXMaterialCreateInfo& createInfo, const MaterialInstance& clone) const
{
	SharedRef<MaterialTemplate> temp = MakeRef<MaterialTemplate>();

	temp->CreateInfo = createInfo;
	temp->MatOp = clone.GetMaterial();
	temp->Name = name;
	temp->ParameterSet = clone.GetInfo()->ShaderParameters;

	return temp;
}

void AQUA_NAMESPACE::MaterialSystem::BuildSparePipelines()
{
	mConfig->mPointPipeline = mConfig->mPipelineBuilder.BuildGraphicsPipeline<HyperSurfacePipeline>(
		HyperSurfaceType::ePoint, glm::uvec2(1024, 1024), mConfig->mHyperSurfaceShader,
		mConfig->mHyperSurfaceCtxFactory.GetContext());

	mConfig->mLinePipeline = mConfig->mPipelineBuilder.BuildGraphicsPipeline<HyperSurfacePipeline>(
		HyperSurfaceType::eLine, glm::uvec2(1024, 1024), mConfig->mHyperSurfaceShader,
		mConfig->mHyperSurfaceCtxFactory.GetContext());

	auto& cache = mConfig->mCache;

	cache[TEMPLATE_POINT] = MakeRef<MaterialTemplate>();
	cache[TEMPLATE_LINE] = MakeRef<MaterialTemplate>();

	cache[TEMPLATE_POINT]->Name = TEMPLATE_POINT;
	cache[TEMPLATE_POINT]->ParameterSet = {};
	cache[TEMPLATE_POINT]->MatOp.GFX = MakeRef(mConfig->mPointPipeline);

	cache[TEMPLATE_LINE]->Name = TEMPLATE_LINE;
	cache[TEMPLATE_LINE]->ParameterSet = {};
	cache[TEMPLATE_LINE]->MatOp.GFX = MakeRef(mConfig->mLinePipeline);
}

AQUA_NAMESPACE::SharedRef<AQUA_NAMESPACE::MaterialTemplate> AQUA_NAMESPACE::
	MaterialSystem::FindTemplate(const std::string& name) const
{
	if (mConfig->mCache.find(name) == mConfig->mCache.end())
		return {};

	return mConfig->mCache[name];
}

std::expected<AQUA_NAMESPACE::MaterialInstance, bool> AQUA_NAMESPACE::
	MaterialSystem::operator[](const std::string& name) const
{
	auto temp = FindTemplate(name);

	if (!temp)
		return std::unexpected(false);

	MaterialInstance instance{};
	InitInstance(*temp, instance);

	return instance;
}
