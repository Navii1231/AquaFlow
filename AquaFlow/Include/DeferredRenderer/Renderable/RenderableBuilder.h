#pragma once
#include "../../Core/AqCore.h"
#include "Renderable.h"

AQUA_BEGIN

class RenderableBuilder
{
public:
	using CopyVertFn = std::function<void(vkLib::GenericBuffer&, const RenderableInfo&)>;
	using CopyIdxFn = std::function<void(vkLib::GenericBuffer&, const RenderableInfo&)>;
	using CopyVertFnMap = std::unordered_map<std::string, CopyVertFn>;

public:

	template <typename IdxFn>
	RenderableBuilder(IdxFn&& idxFn)
		: mIndexCopyFunc(idxFn) {}

	RenderableBuilder(const CopyVertFnMap& vertFn, CopyIdxFn idxFn)
		: mVertexCopyFuncs(vertFn), mIndexCopyFunc(idxFn) {}

	~RenderableBuilder() = default;

	void SetRscPool(vkLib::ResourcePool pool) { mResourcePool = pool; }

	CopyVertFn& operator[](const std::string& name) { return mVertexCopyFuncs[name]; }

	Renderable CreateRenderable(const RenderableInfo& renderableInfo)
		{
		Renderable renderable;
		renderable.Info = renderableInfo;

		renderable.mVertexBuffers.reserve(mVertexCopyFuncs.size());

		for (auto& [name, func] : mVertexCopyFuncs)
		{
			renderable.mVertexBuffers[name] = mResourcePool.CreateGenericBuffer(
				renderableInfo.Usage | vk::BufferUsageFlagBits::eVertexBuffer, 
				vk::MemoryPropertyFlagBits::eHostCoherent);
			func(renderable.mVertexBuffers[name], renderableInfo);
		}

		renderable.mIndexBuffer = mResourcePool.CreateGenericBuffer(
			renderableInfo.Usage | vk::BufferUsageFlagBits::eIndexBuffer,
			vk::MemoryPropertyFlagBits::eHostCoherent);

		mIndexCopyFunc(renderable.mIndexBuffer, renderableInfo);
		
		return renderable;
	}

private:
	vkLib::ResourcePool mResourcePool;

	CopyIdxFn mIndexCopyFunc;
	CopyVertFnMap mVertexCopyFuncs;
};

AQUA_END
