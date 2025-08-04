#include "Core/vkpch.h"
#include "Process/Executor.h"

uint32_t VK_NAMESPACE::VK_CORE::Executor::SubmitWork(
	const vk::SubmitInfo& submitInfo, std::binary_semaphore* signal, std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/)
{
	switch (mAccessType)
	{
		case QueueAccessType::eGeneric:
			return SubmitToQueuesRange(
				0, mFamilyData->Queues.size(), signal, timeOut, submitInfo);
		case QueueAccessType::eWorker:
			return SubmitToQueuesRange(
				mFamilyData->WorkerBeginIndex, mFamilyData->Queues.size(), signal, timeOut, submitInfo);
		default:
			return -1;
	}
}

uint32_t VK_NAMESPACE::VK_CORE::Executor::SubmitWork(vk::CommandBuffer cmdBuffer, std::binary_semaphore* signal,
	std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/)
{
	vk::SubmitInfo submitInfo{};
	submitInfo.setCommandBuffers(cmdBuffer);

	return SubmitWork(submitInfo, signal, timeOut);
}

uint32_t VK_NAMESPACE::VK_CORE::Executor::SubmitWorkRange(const vk::SubmitInfo* begin, const vk::SubmitInfo* end, std::binary_semaphore* signal,
	std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/)
{
	switch (mAccessType)
	{
		case QueueAccessType::eGeneric:
			return SubmitToQueuesRange(
				0, mFamilyData->Queues.size(), signal, timeOut, begin, end);
		case QueueAccessType::eWorker:
			return SubmitToQueuesRange(
				mFamilyData->WorkerBeginIndex, mFamilyData->Queues.size(), signal, timeOut, begin, end);
		default:
			return -1;
	}
}

uint32_t VK_NAMESPACE::VK_CORE::Executor::SubmitWorkRange(
	vk::CommandBuffer* begin, vk::CommandBuffer* end, std::binary_semaphore* signal,
	std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/)
{
	std::vector<vk::SubmitInfo> submitInfos(end - begin);

	for (auto& info : submitInfos)
		info.setCommandBuffers(*(begin++));

	return SubmitWorkRange(submitInfos.begin()._Unwrapped(), submitInfos.end()._Unwrapped(), signal, timeOut);
}

uint32_t VK_NAMESPACE::VK_CORE::Executor::FreeQueue(std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds(0)*/) const
{
	return TraverseIdleQueues(mAccessType == QueueAccessType::eGeneric ? 0 : mFamilyData->WorkerBeginIndex,
		GetQueueCount(), timeOut, [](Core::Ref<Core::Queue> queue)
		{
			return queue->WaitIdleAsync(0) == vk::Result::eSuccess;
		});
}

bool VK_NAMESPACE::VK_CORE::Executor::WaitIdle(
	std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/)
{
	bool AllIdle = true;

	auto CheckIdle = [&AllIdle, timeOut](Ref<Queue> queue)
	{
		if (!queue->WaitIdle(timeOut))
			AllIdle &= false;
	};

	switch (mAccessType)
	{
		case QueueAccessType::eGeneric:
			std::for_each(mFamilyData->Queues.begin(), mFamilyData->Queues.end(), CheckIdle);
			return AllIdle;
		case QueueAccessType::eWorker:
			std::for_each(mFamilyData->Queues.begin() + mFamilyData->WorkerBeginIndex, 
				mFamilyData->Queues.end(), CheckIdle);
			return AllIdle;
		default:
			return false;
	}
}

bool VK_NAMESPACE::VK_CORE::Executor::operator==(const Executor& Other) const
{
	return (mFamilyData == Other.mFamilyData) && (mAccessType == Other.mAccessType);
}

uint32_t VK_NAMESPACE::VK_CORE::Executor::SubmitToQueuesRange(
	size_t begin, size_t end, std::binary_semaphore* signal, std::chrono::nanoseconds timeOut, const vk::SubmitInfo& submitInfo) const
{
	const auto& queueFamily = *mFamilyData;

	return TraverseIdleQueues(begin, end, timeOut,
		[this, submitInfo, signal](const Core::Ref<Core::Queue>& queue)
	{
		if (queue->Submit(submitInfo, signal, std::chrono::nanoseconds(0)))
			return true;

		return false;
	});
}

uint32_t VK_NAMESPACE::VK_CORE::Executor::SubmitToQueuesRange(
	size_t begin, size_t end, std::binary_semaphore* signal, std::chrono::nanoseconds timeOut,
	const vk::SubmitInfo* submitBegin, const vk::SubmitInfo* submitEnd) const
{
	const auto& queueFamily = *mFamilyData;

	return TraverseIdleQueues(begin, end, timeOut,
		[this, submitBegin, submitEnd, signal](const Core::Ref<Core::Queue>& queue)
	{
		if (queue->SubmitRange(submitBegin, submitEnd, signal, std::chrono::nanoseconds(0)))
			return true;

		return false;
	});
}
