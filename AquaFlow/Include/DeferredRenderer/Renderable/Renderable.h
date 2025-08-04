#pragma once
#include "../../Core/AqCore.h"
#include "../../Geometry3D/GeometryConfig.h"

AQUA_BEGIN

struct RenderableInfo
{
	MeshData Mesh;
	vk::BufferUsageFlags Usage = vk::BufferUsageFlagBits::eStorageBuffer;
};

using VertexMap = std::unordered_map<std::string, vkLib::GenericBuffer>;

// not face meta, but we'll implement it at some point
struct VertexMetaData
{
	uint32_t ModelIdx;
	uint32_t MaterialIdx;
	uint32_t ParameterOffset;
	uint32_t Offset = 0;
	uint32_t Stride = sizeof(glm::vec3);
};

struct Renderable
{
public:
	Renderable() = default;
	virtual ~Renderable() = default;

	// renderable CPU buffers
	RenderableInfo Info;

	// Vertex meta data
	VertexMetaData VertexData;

	VertexMap mVertexBuffers;
	vkLib::GenericBuffer mIndexBuffer;

	vkLib::ResourcePool mResourcePool;

	void UpdateMetaData(const std::string& bindingName, uint32_t beginIdx = 0, uint32_t endIdx = std::numeric_limits<uint32_t>::max());
	static void UpdateMetaData(vkLib::GenericBuffer buffer, const VertexMetaData& data, uint32_t beginIdx, uint32_t endIdx);

private:
	friend class RenderableBuilder;

	void CopyMemory(const Renderable& Other);
	void HollowOut();

};

AQUA_END
