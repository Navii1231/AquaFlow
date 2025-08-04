#include "Core/vkpch.h"
#include "Process/Commands.h"

VK_NAMESPACE::CommandBufferAllocator::~CommandBufferAllocator()
{
	DestructionChecksDebug();
}

vk::CommandBuffer VK_NAMESPACE::CommandBufferAllocator::BeginOneTimeCommands(
	vk::CommandBufferLevel level /*= vk::CommandBufferLevel::ePrimary*/) const
{
	vk::CommandBuffer buffer = Allocate(level);

	vk::CommandBufferBeginInfo beginInfo{};
	beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

	buffer.begin(beginInfo);

	return buffer;
}

void VK_NAMESPACE::CommandBufferAllocator::EndOneTimeCommands(
	vk::CommandBuffer CmdBuffer, Core::Executor Executor) const
{
	CmdBuffer.end();

	vk::SubmitInfo submitInfo{};
	submitInfo.setCommandBuffers(CmdBuffer);

	auto QueueIndex = Executor.SubmitWork(submitInfo);
	Executor[QueueIndex]->WaitIdle();

	Free(CmdBuffer);
}

vk::CommandBuffer VK_NAMESPACE::CommandBufferAllocator::Allocate(
	vk::CommandBufferLevel level /*= vk::CommandBufferLevel::ePrimary*/) const
{
	vk::CommandBufferAllocateInfo allocInfo{};
	allocInfo.setCommandPool(mCommandPool->Handle);
	allocInfo.setCommandBufferCount(1);
	allocInfo.setLevel(level);

	vk::CommandBuffer CmdBuffer;

	{
		std::scoped_lock locker(mCommandPool->Lock);
		CmdBuffer = mDevice->allocateCommandBuffers(allocInfo).front();
		AddInstanceDebug(CmdBuffer);
	}

	return CmdBuffer;
}

void VK_NAMESPACE::CommandBufferAllocator::Free(vk::CommandBuffer CmdBuffer) const
{
	std::scoped_lock locker(mCommandPool->Lock);
	RemoveInstanceDebug(CmdBuffer);
	mDevice->freeCommandBuffers(mCommandPool->Handle, CmdBuffer);
}

void VK_NAMESPACE::CommandBufferAllocator::AddInstanceDebug(vk::CommandBuffer CmdBuffer) const
{
#if _DEBUG
	mAllocatedInstances->insert(CmdBuffer);
#endif
}

void VK_NAMESPACE::CommandBufferAllocator::RemoveInstanceDebug(vk::CommandBuffer CmdBuffer) const
{
#if _DEBUG
	auto found = mAllocatedInstances->find(CmdBuffer);
	_STL_ASSERT(found != mAllocatedInstances->end(), "Trying to free an instance of 'vk::CommandBuffer' from "
		"an allocator (a 'vkLib::CommandBufferAllocator' instance) that did not create the buffer\n"
		"You must free the buffer from where it was created");
	mAllocatedInstances->erase(CmdBuffer);
#endif
}

void VK_NAMESPACE::CommandBufferAllocator::DestructionChecksDebug() const
{
#if _DEBUG
	if (mAllocatedInstances.use_count() != 1)
		return;

	_STL_ASSERT(mAllocatedInstances->empty(),
		"All instances of VK_NAMESPACE::CommandBufferAllocator's along its parent CommandReservoir "
		"were deleted before freeing its vk::CommandBuffers");
#endif
}

VK_NAMESPACE::CommandPools::CommandPools(Core::Ref<vk::Device> device,
	const Core::QueueFamilyIndices& indices, vk::CommandPoolCreateFlags flags)
	: mIndices(indices), mCreationFlags(flags), mDevice(device) 
{
	for (auto index : mIndices)
		mCommandPools[index] = CreateAllocator(index);
}

VK_NAMESPACE::Core::Ref<VK_NAMESPACE::Core::CommandPoolData> 
	VK_NAMESPACE::CommandPools::CreateCommandPool(uint32_t index)
{
	vk::CommandPoolCreateInfo createInfo{};
	createInfo.setFlags(mCreationFlags);
	createInfo.setQueueFamilyIndex(index);

	auto Handle = mDevice->createCommandPool(createInfo);
	auto Device = mDevice;

	Core::CommandPoolData Pool;
	Pool.Handle = Handle;

	return Core::CreateRef(Pool, [Device](const Core::CommandPoolData& PoolData)
	{ Device->destroyCommandPool(PoolData.Handle); });
}

const VK_NAMESPACE::CommandBufferAllocator& VK_NAMESPACE::CommandPools::FindCmdPool(uint32_t familyIndex) const
{
	_STL_ASSERT(mIndices.find(familyIndex) != mIndices.end(),
		"Invalid queue family index passed into "
		"'CommandBufferAllocator::FindCmdPool(uint32_t, bool)'!");

	return mCommandPools.at(familyIndex);
}

void VK_NAMESPACE::CommandPools::AssignTrackerDebug(CommandBufferAllocator& Allocator) const
{
#if _DEBUG
	Allocator.mAllocatedInstances = std::make_shared<std::set<vk::CommandBuffer>>();
#endif
}

void VK_NAMESPACE::CommandPools::DoCopyChecksDebug(const CommandPools* Other) const
{
	_STL_ASSERT(mCreationFlags == Other->mCreationFlags,
		"Trying to copy CommandPoolManager's with different creation flags!");

	_STL_ASSERT(*mDevice == *Other->mDevice,
		"Trying to copy CommandPoolManager's created from different devices!");
}

VK_NAMESPACE::CommandBufferAllocator VK_NAMESPACE::CommandPools::CreateAllocator(uint32_t familyIndex)
{
	CommandBufferAllocator Allocator{};
	Allocator.mDevice = mDevice;
	Allocator.mCommandPool = CreateCommandPool(familyIndex);
	Allocator.mFamilyIndex = familyIndex;
	Allocator.mParentReservoir = this;

	AssignTrackerDebug(Allocator);

	return Allocator;
}
