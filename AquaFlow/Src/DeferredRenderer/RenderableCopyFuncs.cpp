#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderable/RenderableBuilder.h"
#include "DeferredRenderer/Pipelines/PipelineConfig.h"

AQUA_BEGIN

RenderableBuilder::CopyVertFnMap sVertexCopyFn =
{
	{ ENTRY_POSITION, [](vkLib::GenericBuffer& buffer, const RenderableInfo& renderableInfo)
	{
		buffer << renderableInfo.Mesh.aPositions;
	} },

	{ ENTRY_NORMAL, [](vkLib::GenericBuffer& buffer, const RenderableInfo& renderableInfo)
	{
		buffer << renderableInfo.Mesh.aNormals;
	} },

	{ ENTRY_TANGENT_SPACE, [](vkLib::GenericBuffer& buffer, const RenderableInfo& renderableInfo)
	{
		buffer.Resize(renderableInfo.Mesh.aPositions.size() * 2 * sizeof(glm::vec3));

		auto* mem = buffer.MapMemory<glm::vec3>(2 * renderableInfo.Mesh.aPositions.size());

		for (size_t i = 0; i < renderableInfo.Mesh.aPositions.size(); i++)
		{
			mem[0] = renderableInfo.Mesh.aTangents[i];
			mem[1] = renderableInfo.Mesh.aBitangents[i];

			mem += 2;
		}

		buffer.UnmapMemory();
	} },

	{ ENTRY_TEXCOORDS, [](vkLib::GenericBuffer& buffer, const RenderableInfo& renderableInfo)
	{
		buffer << renderableInfo.Mesh.aTexCoords;
	} },

	// This is not really necessary...
	{ ENTRY_METADATA, [](vkLib::GenericBuffer& buffer, const RenderableInfo& renderableInfo)
	{
		buffer.Resize(renderableInfo.Mesh.aPositions.size() * sizeof(glm::vec3));

		auto* mem = buffer.MapMemory<glm::vec3>(renderableInfo.Mesh.aPositions.size());

		for (size_t i = 0; i < renderableInfo.Mesh.aPositions.size(); i++)
		{
			*mem = { 0.0f, 0.0f, 0.0f };
			mem++;
		}

		buffer.UnmapMemory();
	} },
};

RenderableBuilder::CopyIdxFn sIndexCopyFn = [](vkLib::GenericBuffer& idxBuf, const RenderableInfo& info)
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
	};

AQUA_END
