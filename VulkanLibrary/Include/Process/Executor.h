#pragma once
#include "Queues.h"
#include "QueueManager.h"

#include "../Core/Ref.h"

VK_BEGIN
VK_CORE_BEGIN

// we could allow the user to insert std::semaphores or std::condition_variables into the submit functions for CPU synchronization
// Once the queue finishes the task, these sync elements are removed from the queues and it's now up to the user to check their values
class Executor
{
public:
	Executor() = default;

	uint32_t SubmitWork(const vk::SubmitInfo& submitInfo, std::binary_semaphore* signal = nullptr,
		std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max());

	uint32_t SubmitWork(vk::CommandBuffer cmdBuffer, std::binary_semaphore* signal = nullptr,
		std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max());

	uint32_t SubmitWorkRange(const vk::SubmitInfo* begin, const vk::SubmitInfo* end, std::binary_semaphore* signal = nullptr,
		std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max());

	uint32_t SubmitWorkRange(vk::CommandBuffer* begin, vk::CommandBuffer* end, std::binary_semaphore* signal = nullptr,
		std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max());

	uint32_t FreeQueue(std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max()) const;

	bool WaitIdle(std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max());

	uint32_t GetFamilyIndex() const { return mFamilyData->Index; }
	size_t GetWorkerQueueCount() const { return mFamilyData->Queues.size() - mFamilyData->WorkerBeginIndex; }
	size_t GetQueueCount() const { return mFamilyData->Queues.size(); }

	auto begin() const { return mFamilyData->Queues.begin(); }
	auto end() const { return mFamilyData->Queues.end(); }

	Ref<Queue> operator[](size_t index) const { return mFamilyData->Queues[index]; }

	bool operator ==(const Executor& Other) const;
	bool operator !=(const Executor& Other) const { return !(*this == Other); }

private:
	const QueueFamily* mFamilyData = nullptr;
	QueueAccessType mAccessType = QueueAccessType::eGeneric;

	friend class ::VK_NAMESPACE::QueueManager;

private:
	Executor(const QueueFamily* familyData, QueueAccessType accessType)
		: mFamilyData(familyData), mAccessType(accessType) {}

	template <typename Fn> 
	uint32_t TraverseIdleQueues(size_t begin, size_t end, std::chrono::nanoseconds timeOut, Fn fn) const;

	uint32_t SubmitToQueuesRange(size_t begin, size_t end, std::binary_semaphore* signal,
		std::chrono::nanoseconds timeOut, const vk::SubmitInfo& submitInfo) const;

	uint32_t SubmitToQueuesRange(size_t begin, size_t end, std::binary_semaphore* signal, std::chrono::nanoseconds timeOut,
		const vk::SubmitInfo* submitBegin, const vk::SubmitInfo* submitEnd) const;
};

template <typename Fn> 
uint32_t Executor::TraverseIdleQueues(size_t begin, size_t end,
	std::chrono::nanoseconds timeOut, Fn fn) const
{
	const auto& queueFamily = *mFamilyData;

	// finding a "free" queue within the given queue family
	while (true)
	{
		std::unique_lock locker(queueFamily.NotifierMutex);

		for (size_t Curr = begin; Curr < end; Curr++)
		{
			if (fn(queueFamily.Queues[Curr]))
				return static_cast<uint32_t>(Curr);
		}

		// if someone notifies this thread before timeout, we will loop again to find out which queue was freed
		std::cv_status status = queueFamily.IdleNotifier.wait_for(locker, timeOut);

		if (status == std::cv_status::timeout)
			return std::numeric_limits<uint32_t>::max();
	}

	return std::numeric_limits<uint32_t>::max();
}

VK_CORE_END
VK_END
