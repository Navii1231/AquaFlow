#pragma once
#include "Buffer.h"

VK_BEGIN

// Boolean specialization
using GenericBuffer = Buffer<bool>;

template<>
class Buffer<bool> : public RecordableResource
{
public:
	using Byte = uint8_t;
	using Type = bool;

public:
	Buffer() = default;

	template <typename Iter>
	void AppendBuf(Iter Begin, Iter End);

	template <typename Iter>
	void SetBuf(Iter Begin, Iter End, size_t Offset = 0);

	template <typename Iter>
	void FetchMemory(Iter Begin, Iter End, size_t Offset = 0);

	template <typename T>
	T* MapMemory(size_t Count, size_t Offset = 0) const;
	void UnmapMemory() const;

	void InsertMemoryBarrier(vk::CommandBuffer commandBuffer, const MemoryBarrierInfo& pipelineBarrierInfo);

	// TODO: Routine can be further optimized
	void TransferOwnership(uint32_t DstQueueFamilyIndex) const;
	void TransferOwnership(vk::QueueFlagBits optimizedCaps) const;

	void Clear() { mChunk.BufferHandles->ElemCount = 0; }
	bool Empty() const { return GetSize() == 0; }

	Core::BufferResource GetBufferChunk() const { return mChunk; }
	const Core::Buffer& GetNativeHandles() const { return *mChunk.BufferHandles; }
	Core::BufferConfig GetBufferConfig() const { return mChunk.BufferHandles->Config; }

	size_t GetSize() const { return mChunk.BufferHandles->ElemCount; }
	vk::DeviceSize GetDeviceSize() const { return mChunk.BufferHandles->ElemCount; }
	size_t GetCapacity() const { return mChunk.BufferHandles->Config.ElemCount; }

	void Reserve(size_t NewCap);
	void Resize(size_t NewSize);
	void ShrinkToFit();

	explicit operator bool() const { return static_cast<bool>(mChunk.BufferHandles); }

private:
	Core::BufferResource mChunk;

private:
	explicit Buffer(Core::BufferResource&& InputChunk)
		: mChunk(std::move(InputChunk)) {
	}

	friend class ResourcePool;

	template <typename V>
	friend void CopyBufferRegions(Buffer<V>& DstBuf, const Buffer<V>& SrcBuf,
		const std::vector<vk::BufferCopy>& CopyRegions);

private:
	// Helper Functions...
	void ScaleCapacity(size_t NewSize);
	void ScaleCapacityWithoutLoss(size_t NewSize);
	void CopyGPU(Core::Buffer& DstBuffer, const Core::Buffer& SrcBuffer,
		const vk::ArrayProxy<vk::BufferCopy>& CopyRegions);

	// Helper methods for ownership transfer
	void ReleaseBuffer(uint32_t dstIndex, vk::Semaphore bufferReleased) const;
	void AcquireBuffer(uint32_t acquiringFamily, vk::Semaphore bufferReleased) const;

	void MakeHollow();
};

template <typename T, typename Cont>
Buffer<T>& operator <<(Buffer<T>& vkBuf, const Cont& cpuBuf)
{
	vkBuf.AppendBuf(cpuBuf.begin(), cpuBuf.end());
	return vkBuf;
}

template <typename T>
Buffer<T>& operator <<(Buffer<T>& vkBuf, const T& cpuVal)
{
	vkBuf.AppendBuf(&cpuVal, &cpuVal + 1);
	return vkBuf;
}

template <typename T, typename Cont>
Buffer<T>& operator >>(Buffer<T>& vkBuf, Cont& cpuBuf)
{
	size_t offset = cpuBuf.size();
	cpuBuf.resize(cpuBuf.size() + vkBuf.GetSize());

	vkBuf.FetchMemory(cpuBuf.begin() + offset, cpuBuf.end());
	return vkBuf;
}

template <typename T>
Buffer<T>& operator >>(Buffer<T>& vkBuf, T& cpuVal)
{
	vkBuf.FetchMemory(&cpuVal, &cpuVal + 1);
	return vkBuf;
}

// Generic Buffer class operators
template <typename Cont>
Buffer<bool>& operator <<(Buffer<bool>& vkBuf, const Cont& cpuBuf)
{
	vkBuf.AppendBuf(cpuBuf.begin(), cpuBuf.end());
	return vkBuf;
}

template <typename Cont>
Buffer<bool>& operator >>(Buffer<bool>& vkBuf, Cont& cpuBuf)
{
	size_t offset = cpuBuf.size();
	cpuBuf.resize(cpuBuf.size() + vkBuf.GetSize() / sizeof(decltype(*cpuBuf.begin())));

	vkBuf.FetchMemory(cpuBuf.begin() + offset, cpuBuf.end());
	return vkBuf;
}

template<typename Iter>
void Buffer<bool>::AppendBuf(Iter Begin, Iter End)
{
	std::span range(Begin, End - Begin);

	static_assert(std::ranges::contiguous_range<decltype(range)>,
		"vkLib::Buffer::SetBuf only accepts contiguous memory");

	SetBuf<Iter>(Begin, End, mChunk.BufferHandles->ElemCount);
}

template<typename Iter>
void Buffer<bool>::SetBuf(Iter Begin, Iter End, size_t Offset)
{
	// no range exists
	if (Begin == End)
		return;

	std::span range(Begin, End - Begin);

	static_assert(std::ranges::contiguous_range<decltype(range)>,
		"vkLib::Buffer::SetBuf only accepts contiguous memory");

	constexpr size_t ElemSize = sizeof(decltype(*Begin));

	// Converting the units to bytes instead of count
	Offset *= ElemSize;
	size_t Count = (End - Begin) * ElemSize;

	Reserve(Count + Offset);

	Byte* Memory = MapMemory<Byte>(Count, Offset);
	std::memcpy(Memory, &(*Begin), Count);
	UnmapMemory();

	mChunk.BufferHandles->ElemCount = Count + Offset;
}

template <typename Iter>
void Buffer<bool>::FetchMemory(Iter Begin, Iter End, size_t Offset /*= 0*/)
{
	std::span range(Begin, End - Begin);

	static_assert(std::ranges::contiguous_range<decltype(range)>,
		"vkLib::Buffer::SetBuf only accepts contiguous memory");

	if (Begin == End)
		return;

	size_t Count = End - Begin;

	Byte* Memory = MapMemory<Byte>(Count * sizeof(Type), Offset * sizeof(Type));
	std::memcpy(&(*Begin), Memory, Count * sizeof(decltype(*Begin)));
	UnmapMemory();
}

template<typename T>
T* Buffer<bool>::MapMemory(size_t Count, size_t Offset) const
{
	return (T*)mChunk.Device->mapMemory(mChunk.BufferHandles->Memory,
		Offset * sizeof(T), Count * sizeof(T));
}

VK_END
