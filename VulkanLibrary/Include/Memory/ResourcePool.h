#pragma once
#include "MemoryConfig.h"
#include "GenericBuffer.h"
#include "Image.h"
#include "../Process/Commands.h"

VK_BEGIN

class ResourcePool
{
public:
	ResourcePool() = default;

	template <typename T, typename ...Properties>
	Buffer<T> CreateBuffer(Properties&&... properties) const;

	template <typename ...Properties>
	GenericBuffer CreateGenericBuffer(Properties&&... properties) const;

	Image CreateImage(const ImageCreateInfo& info) const;

	Core::Ref<vk::Sampler> CreateSampler(const SamplerInfo& samplerInfo, SamplerCache cache = {}) const;

	SamplerCache CreateSamplerCache() const;

	std::shared_ptr<const QueueManager> GetQueueManager() const { return mQueueManager; }

	explicit operator bool() const { return static_cast<bool>(mDevice); }

private:
	Core::Ref<vk::Device> mDevice;
	PhysicalDevice mPhysicalDevice;

	CommandPools mBufferCommandPools;
	CommandPools mImageCommandPools;

	std::shared_ptr<const QueueManager> mQueueManager;

	friend class Context;
};

template <typename T, typename ...Properties>
VK_NAMESPACE::Buffer<T> VK_NAMESPACE::ResourcePool::CreateBuffer(Properties&&... props) const
{
	auto Device = mDevice;

	Core::BufferResource Chunk;
	Core::BufferConfig Config{};

	Config.ElemCount = 0;
	Config.LogicalDevice = *Device;
	Config.PhysicalDevice = mPhysicalDevice.Handle;
	Config.TypeSize = sizeof(T);

	// Creating an empty buffer
	Chunk.BufferHandles = Core::CreateRef(vkLib::Core::Buffer(), [Device](Core::Buffer buffer)
	{
		if (buffer.Handle)
		{
			Device->destroyBuffer(buffer.Handle);
			Device->freeMemory(buffer.Memory);
		}
	});

	Chunk.Device = Device;
	Chunk.BufferHandles->Config = Config;

	(Chunk.BufferHandles->SetProperty(std::forward<Properties>(props)),...);

	Buffer<T> buffer(std::move(Chunk));
	buffer.mQueueManager = GetQueueManager();
	buffer.mCommandPools = mBufferCommandPools;

	buffer.Reserve(Chunk.BufferHandles->ElemCount == 0 ? 1 : Chunk.BufferHandles->ElemCount);
	buffer.Resize(Chunk.BufferHandles->ElemCount);

	return buffer;
}

template <typename... Properties>
VK_NAMESPACE::GenericBuffer VK_NAMESPACE::ResourcePool::CreateGenericBuffer(Properties&&... props) const
{
	return CreateBuffer<GenericBuffer::Type>(std::forward<Properties>(props)...);
}

VK_END
