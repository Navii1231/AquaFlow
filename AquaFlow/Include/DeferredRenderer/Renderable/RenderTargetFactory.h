#pragma once
#include "FactoryConfig.h"

AQUA_BEGIN

enum class RenderFactoryError
{
	eColorOrDepthAttachment              = 0,
	eInconsistentColorAttachments        = 1,
};

class RenderTargetFactory
{
public:
	RenderTargetFactory() = default;

	void Clear();

	void SetContextBuilder(vkLib::RenderContextBuilder builder) { mBuilder = builder; }

	void AddColorAttribute(const std::string& name, const std::string& format);
	void SetDepthAttribute(const std::string& name, const std::string& format);

	template <typename ...Property>
	void SetColorProperties(const std::string& name, Property&&... prop);

	template <typename ...Property>
	void SetAllColorProperties(Property&&... prop);

	template <typename ...Property>
	void SetDepthProperties(Property&&... prop);

	void SetStencilLoadOp(vk::AttachmentLoadOp op);
	void SetStencilStoreOp(vk::AttachmentStoreOp op);

	// Changes the state from preparation to production
	// All functions declared below this one can only be called after the state is validated
	std::expected<bool, RenderFactoryError> Validate();

	// The client isn't required to set every image view, the ones omitted will be
	// created - depth view included - on the fly by the class
	void SetImageView(const std::string& name, vkLib::ImageView view);
	void RemoveImageView(const std::string& name);
	void ClearAllImageViews();

	void SetTargetSize(const glm::uvec2& size) { mTargetSize = size; }

	std::expected<vkLib::Framebuffer, vkLib::FramebufferConstructError> CreateFramebuffer();

	vkLib::RenderTargetContext GetContext() const { return mContext; }

private:
	ImageAttributeList mColorAttributes;
	ImageAttributePropertiesMap mColorProperties;

	ImageAttribute mDepthAttribute;
	ImageAttributeProperties mDepthProperties;

	ImageViewList mImageViews;

	bool mValidated = false;

	glm::uvec2 mTargetSize = { 0, 0 };

	bool mUsingDepth = false, mUsingStencil = false;

	vkLib::RenderTargetContext mContext;
	vkLib::RenderContextBuilder mBuilder;

private:
	// helper functions
	void SetProperty(ImageAttributeProperties& properties, vk::AttachmentLoadOp val) { properties.LoadOp = val; }
	void SetProperty(ImageAttributeProperties& properties, vk::AttachmentStoreOp val) { properties.StoreOp = val; }
	void SetProperty(ImageAttributeProperties& properties, vk::ImageLayout val) { properties.Layout = val; }
	void SetProperty(ImageAttributeProperties& properties, vk::ImageUsageFlags val) { properties.Usage = val; }
	void SetProperty(ImageAttributeProperties& properties, vk::SampleCountFlagBits val) { properties.Samples = val; }
	void SetProperty(ImageAttributeProperties& properties, vk::MemoryPropertyFlags val) { properties.MemProps = val; }

	void GenerateRenderContext();

	void Invalidate() { mValidated = false; }
	std::expected<bool, RenderFactoryError> CheckValidity();
};

template <typename ...Property>
void AQUA_NAMESPACE::RenderTargetFactory::SetColorProperties(const std::string& name, Property&& ...prop)
{
	Invalidate();
		
	auto& props = mColorProperties[name];

	(SetProperty(props, std::forward<Property>(prop)), ...);
}

template <typename ...Property>
void AQUA_NAMESPACE::RenderTargetFactory::SetAllColorProperties(Property&&... prop)
{
	Invalidate();

	for (auto& [name, props] : mColorProperties)
	{
		(SetProperty(props, std::forward<Property>(prop)), ...);
	}
}

template <typename ...Property>
void AQUA_NAMESPACE::RenderTargetFactory::SetDepthProperties(Property&&... prop)
{
	Invalidate();

	(SetProperty(mDepthProperties, std::forward<Property>(prop)), ...);
}


AQUA_END
