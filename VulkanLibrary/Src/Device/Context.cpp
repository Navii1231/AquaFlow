#include "Core/vkpch.h"
#include "Device/Context.h"

#include "../Process/Commands.h"

#include "Core/Utils/DeviceCreation.h"
#include "Core/Utils/FramebufferUtils.h"
#include "Core/Utils/SwapchainUtils.h"

VK_NAMESPACE::Context::Context(const ContextCreateInfo& info)
	: mHandle(), mDeviceInfo(info), mSwapchain()
{
	DoSanityChecks();

	auto Instance = mDeviceInfo.PhysicalDevice.ParentInstance;

	mHandle = Core::Ref(Core::Utils::CreateDevice(mDeviceInfo),
		[Instance](vk::Device device) { device.destroy(); });

	// Retrieving queues from the device...
	auto QueueCapabilities = info.PhysicalDevice.GetQueueIndexMap(info.DeviceCapabilities);
	auto [Found, Indices] = info.PhysicalDevice.GetQueueFamilyIndices(info.DeviceCapabilities);

	Core::QueueFamilyMap<std::pair<size_t, std::vector<Core::Ref<Core::Queue>>>> Queues;

	auto Device = mHandle;

	for (auto index : Indices)
	{
		auto& FamilyProps = info.PhysicalDevice.QueueProps[index];
		auto& [WorkerBegin, FamilyRef] = Queues[index];

		size_t Count = std::min(info.MaxQueueCount, FamilyProps.queueCount);
		WorkerBegin = std::max(1, static_cast<int>(0.2f * Count));

		for (size_t i = 0; i < Count; i++)
		{
			auto Fence = mHandle->createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
			Core::Queue Queue(mHandle->getQueue(index, static_cast<uint32_t>(i)),
				Fence, static_cast<uint32_t>(i), *mHandle);

			FamilyRef.emplace_back(Queue, [Device](Core::Queue queue)
				{ Device->destroyFence(queue.mFence); });
		}
	}

	// Creating a queue manager...
	mQueueManager = std::shared_ptr<QueueManager>(
		new QueueManager(Queues, Indices, QueueCapabilities, 
			mDeviceInfo.PhysicalDevice.QueueProps, mHandle));

	mDescPoolBuilder = { mHandle };
	 
	// Creating the swapchain here...
	CreateSwapchain(info.SwapchainInfo);
}

VK_NAMESPACE::Core::Ref<vk::Semaphore> VK_NAMESPACE::Context::CreateSemaphore() const
{
	auto Device = mHandle;

	return Core::CreateRef(Core::Utils::CreateSemaphore(*mHandle), 
		[Device](vk::Semaphore semaphore) { Device->destroySemaphore(semaphore); });
}

VK_NAMESPACE::Core::Ref<vk::Fence> VK_NAMESPACE::Context::CreateFence(bool Signaled) const
{
	auto Device = mHandle;

	return Core::CreateRef(Core::Utils::CreateFence(*mHandle, Signaled),
		[Device](vk::Fence fence) { Device->destroyFence(fence); });
}

VK_NAMESPACE::Core::Ref<vk::Event> VK_NAMESPACE::Context::CreateEvent() const
{
	auto Device = mHandle;

	return Core::CreateRef(Core::Utils::CreateEvent(*mHandle),
		[Device](vk::Event event_) {Device->destroyEvent(event_); });
}

void VK_NAMESPACE::Context::ResetFence(vk::Fence Fence)
{
	mHandle->resetFences(Fence);
}

void VK_NAMESPACE::Context::ResetEvent(vk::Event Event)
{
	mHandle->resetEvent(Event);
}

void VK_NAMESPACE::Context::WaitForFence(vk::Fence fence, uint64_t timeout /*= UINT64_MAX*/)
{
	vk::Result Temp = mHandle->waitForFences(fence, VK_TRUE, timeout);
}

VK_NAMESPACE::CommandPools VK_NAMESPACE::Context::CreateCommandPools(
	bool IsTransient /*= false*/, bool IsProtected /*= false*/) const
{
	vk::CommandPoolCreateFlags CreationFlags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

	if (IsTransient)
		CreationFlags |= vk::CommandPoolCreateFlagBits::eTransient;

	if (IsProtected)
		CreationFlags |= vk::CommandPoolCreateFlagBits::eProtected;

	return { mHandle, mQueueManager->GetQueueFamilyIndices(), CreationFlags };
}

VK_NAMESPACE::PipelineBuilder VK_NAMESPACE::Context::MakePipelineBuilder() const
{
	PipelineBuilder builder{};
	builder.mDevice = mHandle;
	builder.mResourcePool = CreateResourcePool();

	PipelineBuilderData data{};
	vk::PipelineCacheCreateInfo cacheInfo{};
	data.Cache = mHandle->createPipelineCache(cacheInfo);

	auto Device = mHandle;

	builder.mData = Core::CreateRef(data, [Device](const PipelineBuilderData& builderData)
	{
		Device->destroyPipelineCache(builderData.Cache);
	});

	builder.mDevice = mHandle;
	builder.mDescPoolManager = FetchDescriptorPoolManager();

	return builder;
}

VK_NAMESPACE::DescriptorPoolManager VK_NAMESPACE::Context::FetchDescriptorPoolManager() const
{
	DescriptorPoolManager manager;
	manager.mPoolBuilder = mDescPoolBuilder;

	return manager;
}

VK_NAMESPACE::ResourcePool VK_NAMESPACE::Context::CreateResourcePool() const
{
	ResourcePool pool{};
	pool.mDevice = mHandle;
	pool.mPhysicalDevice = mDeviceInfo.PhysicalDevice;
	pool.mBufferCommandPools = CreateCommandPools(true);
	pool.mImageCommandPools = CreateCommandPools(true);
	pool.mQueueManager = mQueueManager;

	return pool;
}

VK_NAMESPACE::RenderContextBuilder VK_NAMESPACE::Context::FetchRenderContextBuilder(vk::PipelineBindPoint bindPoint)
{
	RenderContextBuilder Context;
	Context.mDevice = mHandle;
	Context.mPhysicalDevice = mDeviceInfo.PhysicalDevice.Handle;
	Context.mQueueManager = GetQueueManager();
	Context.mCommandPools = CreateCommandPools(true);
	Context.mBindPoint = bindPoint;

	return Context;
}

void VK_NAMESPACE::Context::InvalidateSwapchain(const SwapchainInvalidateInfo& newInfo)
{
	SwapchainInfo swapchainInfo{};
	swapchainInfo.Width = newInfo.Width;
	swapchainInfo.Height = newInfo.Height;
	swapchainInfo.PresentMode = newInfo.PresentMode;
	swapchainInfo.Surface = mSwapchain->mInfo.Surface;

	CreateSwapchain(swapchainInfo);
}

void VK_NAMESPACE::Context::CreateSwapchain(const SwapchainInfo& info)
{
	mSwapchain = std::shared_ptr<Swapchain>(new Swapchain(mHandle, mDeviceInfo.PhysicalDevice, 
		FetchRenderContextBuilder(vk::PipelineBindPoint::eGraphics), info, 
		GetQueueManager(), CreateCommandPools()));
}

void VK_NAMESPACE::Context::DoSanityChecks()
{
	_STL_ASSERT(*mDeviceInfo.SwapchainInfo.Surface,
		"Device requires a Vulkan Surface to create swapchain\n");

	auto& extensions = mDeviceInfo.Extensions;
	auto found = std::find(extensions.begin(), extensions.end(), VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	if (found == extensions.end())
		extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}
