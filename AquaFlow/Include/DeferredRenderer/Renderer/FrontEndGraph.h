#pragma once
#include "Features.h"
#include "Environment.h"
#include "../Renderable/RenderTargetFactory.h"
#include "../../Execution/GraphBuilder.h"

AQUA_BEGIN

struct FrontEndGraphConfig;

class FrontEndGraph
{
private:
	FrontEndGraph();
	~FrontEndGraph() = default;

	void SetCtx(vkLib::Context ctx);

	void SetEnvironment(EnvironmentRef env);
	void SetFeaturesInfos(const FeatureInfoMap& featureInfo);

	void SetFeatureFlags(RendererFeatureFlags flags);
	void SetShadowFeature(const ShadowCascadeFeature& feature);

	void SetVertexFactory(VertexFactory& factory);

	void PrepareFeatures();
	void PrepareDepthCascades();
	void CreateGraph();

	void SetModels(Mat4Buf models);
	void UpdateGraph();

	void PrepareFramebuffers(const glm::uvec2& rendererResolution);
	void PrepareDepthBuffers();

	EXEC_NAMESPACE::Graph GetGraph() const;
	EXEC_NAMESPACE::GraphList GetGraphList() const;

	std::vector<std::string> GetOutputs() const;

	std::vector<vkLib::ImageView> GetDepthViews() const;
	std::vector <vkLib::Framebuffer> GetDepthbuffers() const;

private:
	SharedRef<FrontEndGraphConfig> mConfig;

	friend class Renderer;
	friend struct RendererConfig;
};

AQUA_END
