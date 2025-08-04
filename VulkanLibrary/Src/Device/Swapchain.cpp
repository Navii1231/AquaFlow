#include "Core/vkpch.h"
#include "Device/Swapchain.h"
#include "../Core/Utils/SwapchainUtils.h"

VK_NAMESPACE::Swapchain::Swapchain(Core::Ref<vk::Device> device, PhysicalDevice physicalDevice,
	const RenderContextBuilder& builder, const SwapchainInfo& info, 
	QueueManagerRef queueManager, CommandPools commandPools)
	: mDevice(device), mRenderContextBuilder(builder), 
	mInfo(info), mQueueManager(queueManager), mCommandPools(commandPools), mPhysicalDevice(physicalDevice)
{
	const auto& QueueCapabilites = queueManager->GetFamilyIndicesByCapabilityMap();
	auto Device = mDevice;

	uint32_t GraphicsFamilyIndex = *QueueCapabilites.at(vk::QueueFlagBits::eGraphics).begin();

	auto Bundle = Core::Utils::CreateSwapchain(*mDevice, info, mPhysicalDevice);

	mHandle = Core::CreateRef(Bundle.Handle, 
		[Device](vk::SwapchainKHR swapchain) { Device->destroySwapchainKHR(swapchain); });

	mData.CommandPoolManager = mCommandPools;
	mData.CommandsAllocator = mCommandPools[GraphicsFamilyIndex];

	// Creating ImageViews and Framebuffers here...
	uint32_t FrameCount = Bundle.ImageCount;

	RenderContextCreateInfo contextInfo{};
	contextInfo.Attachments.resize(1);
	contextInfo.Attachments[0].Format = Bundle.ImageFormat;
	contextInfo.Attachments[0].Usage = vk::ImageUsageFlagBits::eColorAttachment |
		vk::ImageUsageFlagBits::eTransferDst;

	contextInfo.Attachments[0].Layout = vk::ImageLayout::eColorAttachmentOptimal;

	mData.TargetContext = mRenderContextBuilder.Build(contextInfo);

	auto RenderPass = mData.TargetContext.GetInfo().RenderPass;

	auto Images = Device->getSwapchainImagesKHR(*mHandle);

	auto& Frames = mData.Frames;
	Frames.resize(FrameCount);

	auto SwapchainHandle = mHandle;

	mData.ImageAcquired = Core::CreateRef(Device->createSemaphore({}),
		[Device](vk::Semaphore semaphore) { Device->destroySemaphore(semaphore); });

	for (uint32_t i = 0; i < FrameCount; i++)
	{
		auto CommandsAlloc = mData.CommandsAllocator;

		ImageViewCreateInfo viewInfo{};
		viewInfo.Type = vk::ImageViewType::e2D;
		viewInfo.Subresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		viewInfo.Subresource.baseArrayLayer = 0;
		viewInfo.Subresource.baseMipLevel = 0;
		viewInfo.Subresource.layerCount = 1;
		viewInfo.Subresource.levelCount = 1;
		viewInfo.Format = Bundle.ImageFormat;

		vk::ImageView imageViewHandle = Core::Utils::CreateImageView(*Device, Images[i], viewInfo);

		Image currImage{};
		InsertImageHandle(currImage, Images[i], imageViewHandle, Bundle);
		currImage.mChunk->ImageHandles.IdentityViewInfo = viewInfo;

		Framebuffer renderTarget{};
		FramebufferInfo data;

		data.ColorAttachments.push_back(currImage.GetIdentityImageView());

		vk::ImageView view = data.ColorAttachments[0].GetNativeHandle();

		vk::FramebufferCreateInfo framebufferInfo{};
		framebufferInfo.setAttachments(view);
		framebufferInfo.setWidth(Bundle.ImageExtent.width);
		framebufferInfo.setHeight(Bundle.ImageExtent.height);
		framebufferInfo.setLayers(1);
		framebufferInfo.setRenderPass(RenderPass);

		data.Handle = Device->createFramebuffer(framebufferInfo);
		data.Width = Bundle.ImageExtent.width;
		data.Height = Bundle.ImageExtent.height;
		data.ParentContext = mData.TargetContext;

		renderTarget.mInfo = Core::CreateRef(data, [Device](const FramebufferInfo& data)
		{
			Device->destroyFramebuffer(data.Handle);
			Device->destroyImageView(data.ColorAttachments[0].GetNativeHandle());
		});

		renderTarget.mDevice = Device;
		renderTarget.mQueueManager = queueManager;
		renderTarget.mCommandPools = mCommandPools;

		renderTarget.TransitionColorAttachmentLayouts(vk::ImageLayout::eColorAttachmentOptimal,
			vk::PipelineStageFlagBits::eColorAttachmentOutput);

		SwapchainFrame Frame(std::move(renderTarget));

		Frame.ImageRendered = mDevice->createSemaphore({});
		Frame.CmdBuffer = CommandsAlloc.Allocate();

		Frames[i] = Core::CreateRef(Frame, [Device, SwapchainHandle, CommandsAlloc](const SwapchainFrame& frame)
		{
			Device->destroySemaphore(frame.ImageRendered);
			CommandsAlloc.Free(frame.CmdBuffer);
		});
	}
}

void VK_NAMESPACE::Swapchain::InsertImageHandle(Image& image, 
	vk::Image handle, vk::ImageView viewHandle, const Core::SwapchainBundle& bundle)
{
	Core::ImageResource chunk;
	chunk.Device = mDevice;

	Core::Image handles{};
	handles.Handle = handle;

	handles.Config.CurrLayout = vk::ImageLayout::eUndefined;
	handles.Config.Extent = vk::Extent3D(bundle.ImageExtent.width, bundle.ImageExtent.height, 1);
	handles.Config.Format = bundle.ImageFormat;
	handles.Config.LogicalDevice = *mDevice;
	handles.Config.PhysicalDevice = mPhysicalDevice.Handle;
	handles.Config.PrevStage = vk::PipelineStageFlagBits::eTopOfPipe;
	handles.Config.ResourceOwner = 0;
	handles.Config.Type = vk::ImageType::e2D;
	handles.Config.ViewType = vk::ImageViewType::e2D;
	handles.Config.Usage = vk::ImageUsageFlagBits::eColorAttachment;

	handles.IdentityView = viewHandle;
	handles.Memory = nullptr;
	handles.MemReq = vk::MemoryRequirements();

	auto SwapchainHandle = mHandle;
	chunk.ImageHandles = handles;

	::new(&image) Image();
	image.mChunk = Core::CreateRef(chunk, [SwapchainHandle](const Core::ImageResource& imageChunk)
	{
		// Do nothing, resource is owned by the swapchain...
	});

	image.mQueueManager = GetQueueManager();
	image.mCommandPools = mCommandPools;
}
