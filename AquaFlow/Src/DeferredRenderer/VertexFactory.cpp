#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderable/VertexFactory.h"

void AQUA_NAMESPACE::VertexFactory::Reset()
{
	mVertexBindings.clear();
	mVertexResources.clear();
	mVertexBindingProperties.clear();
}

void AQUA_NAMESPACE::VertexFactory::SetVertexBindings(const VertexBindingMap& map)
{
	mVertexBindings = map;

	Initialize();

}

void AQUA_NAMESPACE::VertexFactory::SetIndexProperties(vk::BufferUsageFlags usageFlags, vk::MemoryPropertyFlags memProps)
{
	mIndexUsage = usageFlags;
	mIndexMemProps = memProps;
}

void AQUA_NAMESPACE::VertexFactory::SetVertexProperties(const std::string& name, 
	vk::BufferUsageFlags usageFlags, vk::MemoryPropertyFlags memProps)
{
	mVertexBindingProperties[name].Usage = usageFlags;
	mVertexBindingProperties[name].MemProps = memProps;
}

void AQUA_NAMESPACE::VertexFactory::SetAllVertexProperties(vk::BufferUsageFlags usageFlags, vk::MemoryPropertyFlags memProps)
{
	for (auto& [name, binding] : mVertexBindingProperties)
	{
		binding.Usage = usageFlags;
		binding.MemProps = memProps;
	}
}

std::expected<bool, AQUA_NAMESPACE::VertexError> AQUA_NAMESPACE::VertexFactory::Validate()
{
	// Checking if the locations do not repeat
	// Checking if all the properties are coherent in all maps

	if (!CheckVertexBindings(mVertexBindings))
		return std::unexpected(VertexError::eDuplicateVertexLocations);

	if (!CheckForUniqueNames())
		return std::unexpected(VertexError::eDuplicateBindingNames);

	mVertexInputStream = GenerateVertexInputStreamInfo(mVertexBindings);

	TraverseBuffers([this](uint32_t idx, const std::string& name, vkLib::GenericBuffer& buffer)
		{
			buffer = mResourcePool.CreateGenericBuffer(mVertexBindingProperties[name].Usage |
				vk::BufferUsageFlagBits::eVertexBuffer, mVertexBindingProperties[name].MemProps);
		});

	mIndexBuffer = mResourcePool.CreateGenericBuffer(mIndexUsage | vk::BufferUsageFlagBits::eIndexBuffer, mIndexMemProps);

	ReserveVertices(sDefaultVertexCount);
	ReserveIndices(sDefaultIndexCount);

	return true;
}

void AQUA_NAMESPACE::VertexFactory::ClearBuffers()
{
	TraverseBuffers([](uint32_t idx, const std::string& name, vkLib::GenericBuffer buffer)
		{
			buffer.Clear();
		});

	if(mIndexBuffer)
		mIndexBuffer.Clear();
}

void AQUA_NAMESPACE::VertexFactory::ReserveVertices(uint32_t count)
{
	TraverseBuffers([this, count](uint32_t idx, const std::string& name, vkLib::GenericBuffer buffer)
		{
			uint32_t stride = mVertexInputStream.Bindings[idx].stride;
			buffer.Reserve(count * stride);
		});
}

void AQUA_NAMESPACE::VertexFactory::ReserveIndices(uint32_t count)
{
	mIndexBuffer.Reserve(count * sizeof(uint32_t));
}

vkLib::VertexInputDesc AQUA_NAMESPACE::VertexFactory::GenerateVertexInputStreamInfo(const VertexBindingMap& bindings)
{
	vkLib::VertexInputDesc desc{};
	desc.Bindings.reserve(bindings.size());

	for (const auto&[idx, binding] : bindings)
	{
		uint32_t stride = 0;

		for (uint32_t attribIdx = 0; attribIdx < binding.Attributes.size(); attribIdx++)
		{
			const auto& [format, layout, whatever, bytes] = ConvertIntoTagAttributes(binding.Attributes[attribIdx].Format);

			desc.Attributes.emplace_back(binding.Attributes[attribIdx].Location, idx, format, stride);

			stride += bytes;
		}

		desc.Bindings.emplace_back(idx, stride, binding.InputRate);
	}

	return desc;
}

bool AQUA_NAMESPACE::VertexFactory::CheckVertexBindings(VertexBindingMap vertexBindings)
{
	std::unordered_set<uint32_t> locations{};

	for (const auto& [idx, attributes] : vertexBindings)
	{
		for (const auto& attribute : attributes.Attributes)
		{
			if (locations.find(attribute.Location) != locations.end())
				return false;

			locations.insert(attribute.Location);
		}
	}

	return true;
}

void AQUA_NAMESPACE::VertexFactory::CopyVertexBuffer(vk::CommandBuffer cmd, vkLib::GenericBuffer dst, vkLib::GenericBuffer src)
{
	vk::BufferCopy copyRegion{};
	copyRegion.setDstOffset(dst.GetSize());
	copyRegion.setSrcOffset(0);
	copyRegion.setSize(src.GetSize());

	dst.Resize(copyRegion.dstOffset + copyRegion.size);

	// using GPU to copy the memory...
	vkLib::RecordCopyBufferRegions(cmd, dst, src, { copyRegion });
}

void AQUA_NAMESPACE::VertexFactory::Initialize()
{
	// Initialize everything...

	for (const auto& [idx, binding] : mVertexBindings)
	{
		mVertexBindingProperties[binding.Name] = {};
		mVertexResources[binding.Name] = {};
	}
}

bool AQUA_NAMESPACE::VertexFactory::CheckForUniqueNames()
{
	std::unordered_set<std::string> uniqueNames;

	for (const auto& [idx, binding] : mVertexBindings)
	{
		if (uniqueNames.find(binding.Name) != uniqueNames.end())
			return false;

		uniqueNames.insert(binding.Name);
	}

	return true;
}

