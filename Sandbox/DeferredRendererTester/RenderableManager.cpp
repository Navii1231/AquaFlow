#include "RenderableManager.h"
#include "DeferredRenderer/Pipelines/PipelineConfig.h"
#include "Geometry3D/MeshLoader.h"

void RenderableManager::LoadModel(const std::string& name, const glm::mat4& model)
{
	AquaFlow::MeshLoader loader(aiPostProcessSteps::aiProcess_CalcTangentSpace);

	std::string blenderModelGeom = "C:\\Users\\Navjot Singh\\Desktop\\Blender Models\\";

	AquaFlow::Geometry3D geom = loader.LoadModel(blenderModelGeom + name + ".obj");

	mModels3D[name] = geom;
	mRenderables[name].resize(geom.GetMeshData().size());

	size_t idx = 0;

	for (const auto& mesh : geom)
	{
		AquaFlow::RenderableInfo renderableInfo{};
		renderableInfo.Mesh = mesh;
		renderableInfo.Usage |= vk::BufferUsageFlagBits::eStorageBuffer;

		mRenderables[name][idx] = mRenderableBuilder->CreateRenderable(renderableInfo);
		mRenderables[name][idx].VertexData.ModelIdx = static_cast<uint32_t>(mModelMatrices.size());

		mModelMatrices.emplace_back(model);

		idx++;
	}
}

void RenderableManager::PrepareRenderableBuilder()
{
	AquaFlow::RenderableBuilder::CopyVertFnMap vertexMap{};

	mRenderableBuilder = std::make_shared<AquaFlow::RenderableBuilder>([](vkLib::GenericBuffer idxBuf, const AquaFlow::RenderableInfo& info)
		{
			idxBuf.Resize(3 * info.Mesh.aFaces.size() * sizeof(uint32_t));

			auto* mem = idxBuf.MapMemory<uint32_t>(3 * info.Mesh.aFaces.size());

			for (size_t i = 0; i < info.Mesh.aFaces.size(); i++)
			{
				mem[0] = info.Mesh.aFaces[i].Indices[0];
				mem[1] = info.Mesh.aFaces[i].Indices[1];
				mem[2] = info.Mesh.aFaces[i].Indices[2];

				mem += 3;
			}

			idxBuf.UnmapMemory();
		});

	mRenderableBuilder->SetRscPool(mContext.CreateResourcePool());

	auto& builder = *mRenderableBuilder;

	builder[ENTRY_POSITION] = [](vkLib::GenericBuffer buf, const AquaFlow::RenderableInfo& info)
		{
			buf << info.Mesh.aPositions;
		};

	builder[ENTRY_NORMAL] = [](vkLib::GenericBuffer buf, const AquaFlow::RenderableInfo& info)
		{
			buf << info.Mesh.aNormals;
		};

	builder[ENTRY_TANGENT_SPACE] = [](vkLib::GenericBuffer buf, const AquaFlow::RenderableInfo& info)
		{
			buf.Resize(info.Mesh.aPositions.size() * 2 * sizeof(glm::vec3));

			auto* mem = buf.MapMemory<glm::vec3>(2 * info.Mesh.aPositions.size());

			for (size_t i = 0; i < info.Mesh.aPositions.size(); i++)
			{
				mem[0] = info.Mesh.aTangents[i];
				mem[1] = info.Mesh.aBitangents[i];

				mem += 2;
			}

			buf.UnmapMemory();
		};

	builder[ENTRY_TEXCOORDS] = [](vkLib::GenericBuffer buf, const AquaFlow::RenderableInfo& info)
		{
			buf << info.Mesh.aTexCoords;
		};

	builder[ENTRY_METADATA] = [this](vkLib::GenericBuffer buf, const AquaFlow::RenderableInfo& info)
		{
			buf.Resize(info.Mesh.aPositions.size() * sizeof(glm::vec3));

			auto* mem = buf.MapMemory<glm::vec3>(info.Mesh.aPositions.size());

			for (size_t i = 0; i < info.Mesh.aPositions.size(); i++)
			{
				*mem = { static_cast<uint32_t>(mModelMatrices.size()), 0.0f, 0.0f };
				mem++;
			}

			buf.UnmapMemory();
		};
}
