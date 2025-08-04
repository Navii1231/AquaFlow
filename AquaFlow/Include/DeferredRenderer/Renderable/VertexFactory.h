#pragma once
#include "FactoryConfig.h"

AQUA_BEGIN

// TODO: #volatile to #future_changes
// For now, it's working more like a #state_machine
class VertexFactory
{
public:
	VertexFactory() = default;
	virtual ~VertexFactory() = default;

	vkLib::VertexInputDesc GetVertexInputStreamInfo() const { return mVertexInputStream; }

	void SetResourcePool(vkLib::ResourcePool pool) { mResourcePool = pool; }

	// operations at binding level
	void Reset();
	void SetVertexBindings(const VertexBindingMap& map);

	void SetIndexProperties(vk::BufferUsageFlags usageFlags, vk::MemoryPropertyFlags memProps);
	void SetVertexProperties(const std::string& name, vk::BufferUsageFlags usageFlags, vk::MemoryPropertyFlags memProps);
	void SetAllVertexProperties(vk::BufferUsageFlags usageFlags, vk::MemoryPropertyFlags memProps);

	// State validation and vertex buffer generation
	std::expected<bool, VertexError> Validate();
	void ClearBuffers();

	template<typename Fn>
	void TraverseBuffers(Fn&& fn);

	void ReserveVertices(uint32_t count);
	void ReserveIndices(uint32_t count);

	// buffer access
	vkLib::GenericBuffer operator[](const std::string& name) const { return mVertexResources.at(name); }
	vkLib::GenericBuffer operator[](const std::string& name) { return mVertexResources[name]; }

	vkLib::GenericBuffer GetIndexBuffer() const { return mIndexBuffer; }
	const VertexBindingMap& GetVertexBindings() const { return mVertexBindings; }
									    
	// static function to generate vertex stream info using vertex binding map
	static vkLib::VertexInputDesc GenerateVertexInputStreamInfo(const VertexBindingMap& bindings);
	static bool CheckVertexBindings(VertexBindingMap vertexBindings);

	static void CopyVertexBuffer(vk::CommandBuffer cmd, vkLib::GenericBuffer dst, vkLib::GenericBuffer src);

private:
	VertexBindingMap mVertexBindings;
	VertexBindingPropertiesMap mVertexBindingProperties;
	VertexResourceMap mVertexResources;

	vkLib::GenericBuffer mIndexBuffer;

	vk::BufferUsageFlags mIndexUsage = vk::BufferUsageFlagBits::eIndexBuffer;
	vk::MemoryPropertyFlags mIndexMemProps = vk::MemoryPropertyFlagBits::eDeviceLocal;

	// TODO: For now, this is fixed all across the application
	vk::IndexType mIndexType = vk::IndexType::eUint32;
	vkLib::VertexInputDesc mVertexInputStream;

	vk::CommandBuffer mCommandBuffer;
	vkLib::ResourcePool mResourcePool;

	constexpr static uint32_t sDefaultVertexCount = 100;
	constexpr static uint32_t sDefaultIndexCount = 100;

private:
	void Initialize();
	bool CheckForUniqueNames();
};

template<typename Fn>
void AQUA_NAMESPACE::VertexFactory::TraverseBuffers(Fn&& fn)
{
	for (const auto&[idx, attribute] : mVertexBindings)
	{
		fn(static_cast<uint32_t>(idx), attribute.Name, mVertexResources[attribute.Name]);
	}
}

AQUA_END
