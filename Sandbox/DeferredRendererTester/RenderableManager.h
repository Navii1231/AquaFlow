#pragma once
#include "DeferredRenderer/Renderable/RenderableBuilder.h"
#include "DeferredRenderer/Renderable/Renderable.h"

class RenderableManager
{
public:
	RenderableManager(vkLib::Context ctx)
		: mContext(ctx) { PrepareRenderableBuilder(); }

	void LoadModel(const std::string& name, const glm::mat4& model);

	const std::vector<glm::mat4> GetModelMatrices() const { return mModelMatrices; }

	const std::vector<AquaFlow::Renderable>& operator[](const std::string& name) const { return mRenderables.at(name); }

private:
	vkLib::ResourcePool mResourcePool;
	std::shared_ptr<AquaFlow::RenderableBuilder> mRenderableBuilder;

	std::unordered_map<std::string, AquaFlow::Geometry3D> mModels3D;
	std::unordered_map<std::string, std::vector<AquaFlow::Renderable>> mRenderables;

	std::vector<glm::mat4> mModelMatrices;

	vkLib::Context mContext;

private:
	void PrepareRenderableBuilder();
};
