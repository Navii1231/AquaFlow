#pragma once
#include "Features.h"
#include "../Renderable/RenderTargetFactory.h"
#include "../../Execution/GraphBuilder.h"
#include "Environment.h"

AQUA_BEGIN

struct BackEndGraphConfig;

class BackEndGraph
{
private:
	BackEndGraph();
	~BackEndGraph() = default;

	void SetCtx(vkLib::Context ctx);

	void SetEnvironment(EnvironmentRef env);

	void SetFeaturesInfos(const FeatureInfoMap& featureInfo);
	void SetFeatureFlags(RendererFeatureFlags flags);

	void PrepareFeatures();
	void PrepareSSAO();
	void PrepareBloomEffect();
	void PreparePostProcessing();
	void CreateGraph();

	void SetModels(Mat4Buf models);
	void UpdateGraph();

	void PrepareFramebuffers(const glm::uvec2& rendererResolution);
	void SetShadingFrameBuffer(vkLib::Framebuffer framebuffer);

	EXEC_NAMESPACE::Graph GetGraph() const;
	EXEC_NAMESPACE::GraphList GetGraphList() const;

	std::vector<std::string> GetInputs() const;

	vkLib::Framebuffer GetPostprocessbuffer() const;

private:
	SharedRef<BackEndGraphConfig> mConfig;

	friend class Renderer;
	friend struct RendererConfig;
};

AQUA_END
