#include "Core/vkpch.h"
#include "Process/Queues.h"

VK_NAMESPACE::VK_CORE::Queue::Queue(const Queue& Other)
	: mHandle(Other.mHandle), mFence(Other.mFence), mQueueIndex(Other.mQueueIndex),
	mFamilyInfo(Other.mFamilyInfo), mDevice(Other.mDevice), mProcessFinished(nullptr) {}

vk::Result VK_NAMESPACE::VK_CORE::Queue::WaitIdleAsync(uint64_t timeout) const
{
	vk::Result result = mDevice.waitForFences(mFence, VK_TRUE, timeout);

	if(result == vk::Result::eSuccess && mProcessFinished)
		mProcessFinished->release();

	return result;
}

VK_NAMESPACE::VK_CORE::Queue& VK_NAMESPACE::VK_CORE::Queue::operator=(const Queue& Other)
{
	std::scoped_lock locker(mLock);

	mHandle = Other.mHandle;
	mFence = Other.mFence;
	mDevice = Other.mDevice;
	mFamilyInfo = Other.mFamilyInfo;
	mQueueIndex = Other.mQueueIndex;

	return *this;
}

bool VK_NAMESPACE::VK_CORE::Queue::Submit(const vk::SubmitInfo& submitInfo, std::binary_semaphore* semaphore,
	std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/) const
{
	std::scoped_lock locker(mLock);

	// making sure the previous work is finished before continuing
	// it also informs the previous user by signaling the user provided semaphore
	if (WaitIdleAsync(timeOut.count()) != vk::Result::eSuccess)
		return false;

	// here the user semaphore can be (re)submitted
	// we acquire the semaphore after the queue is free from previous operation
	if(semaphore)
		semaphore->acquire();

	mProcessFinished = semaphore;

	mDevice.resetFences(mFence);
	mHandle.submit(submitInfo, mFence);

	return true;
}

bool VK_NAMESPACE::VK_CORE::Queue::Submit(vk::Semaphore signalSemaphore,
	vk::CommandBuffer buffer, std::binary_semaphore* semaphore, std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/)
{
	vk::SubmitInfo submitInfo{};
	submitInfo.setCommandBuffers(buffer);
	submitInfo.setSignalSemaphores(signalSemaphore);

	return Submit(submitInfo, semaphore, timeOut);
}

bool VK_NAMESPACE::VK_CORE::Queue::Submit(const QueueWaitingPoint& waitPoint,
	vk::Semaphore signalSemaphore, vk::CommandBuffer buffer, std::binary_semaphore* semaphore,
	std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/)
{
	vk::PipelineStageFlags WaitStages = { waitPoint.WaitDst };

	vk::SubmitInfo submitInfo{};
	submitInfo.setCommandBuffers(buffer);
	submitInfo.setWaitDstStageMask(WaitStages);
	submitInfo.setWaitSemaphores(waitPoint.WaitSemaphore);

	if(signalSemaphore)
		submitInfo.setSignalSemaphores(signalSemaphore);

	return Submit(submitInfo, semaphore, timeOut);
}

bool VK_NAMESPACE::VK_CORE::Queue::Submit(vk::CommandBuffer buffer, std::binary_semaphore* semaphore,
	std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/) const
{
	vk::SubmitInfo submitInfo{};
	submitInfo.setCommandBuffers(buffer);

	return Submit(submitInfo, semaphore, timeOut);
}

bool VK_NAMESPACE::VK_CORE::Queue::SubmitRange(
	const vk::SubmitInfo* Begin, const vk::SubmitInfo* End, std::binary_semaphore* semaphore,
	std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/) const
{
	std::vector<vk::SubmitInfo> submitInfos(Begin, End);

	std::scoped_lock locker(mLock);

	// making sure the previous work is finished before continuing
	// it also informs the previous user by signaling the user provided semaphore
	if (WaitIdleAsync(timeOut.count()) != vk::Result::eSuccess)
		return false;

	// here the user semaphore can be (re)submitted
	// we acquire the semaphore after the queue is free from previous operation
	if (semaphore)
		semaphore->acquire();

	mProcessFinished = semaphore;

	mDevice.resetFences(mFence);
	mHandle.submit(submitInfos);

	return true;
}

bool VK_NAMESPACE::VK_CORE::Queue::SubmitRange(vk::CommandBuffer* Begin, vk::CommandBuffer* End, std::binary_semaphore* semaphore,
	std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/) const
{
	std::vector<vk::SubmitInfo> submitInfos(End - Begin);

	for (auto& info : submitInfos)
		info.setCommandBuffers(*(Begin++));
	
	return SubmitRange(submitInfos.data(), submitInfos.data() + submitInfos.size(), semaphore, timeOut);
}

bool VK_NAMESPACE::VK_CORE::Queue::BindSparse(const vk::BindSparseInfo& bindSparseInfo,
	std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/) const
{
	std::scoped_lock locker(mLock);

	if (WaitIdleAsync(timeOut.count()) != vk::Result::eSuccess)
		return false;

	mDevice.resetFences(mFence);
	mHandle.bindSparse(bindSparseInfo, mFence);

	return true;
}

vk::Result VK_NAMESPACE::VK_CORE::Queue::PresentKHR(const vk::PresentInfoKHR& presentInfo) const
{
	return mHandle.presentKHR(presentInfo);
}

bool VK_NAMESPACE::VK_CORE::Queue::WaitIdle(std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/)const
{
	auto Result = WaitIdleAsync(timeOut.count());

	// Notify any thread, in case, it's stuck waiting for the resource to be free
	// --> The notification is received by Core::Executor::SubmitWork(...)
	if (Result == vk::Result::eSuccess)
	{
		std::scoped_lock locker(mFamilyInfo->NotifierMutex);
		mFamilyInfo->IdleNotifier.notify_one();
	}

	return Result == vk::Result::eSuccess;
}
