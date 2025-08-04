#include "Core/Aqpch.h"
#include "MaterialSystem/MaterialInstance.h"

void AQUA_NAMESPACE::MaterialInstance::UpdateShaderParBuffer()
{
	if (mShaderParBuffer.GetSize() == 0)
		return;

	vkLib::StorageBufferWriteInfo bufferInfo{};
	bufferInfo.Buffer = mShaderParBuffer.GetNativeHandles().Handle;
	const vkLib::BasicPipeline& pipeline = *GetBasicPipeline();

	if (!pipeline.GetShader().IsEmpty(mInfo->ParameterLocation.SetIndex, mInfo->ParameterLocation.Binding))
		pipeline.UpdateDescriptor(mInfo->ParameterLocation, bufferInfo);
}

void AQUA_NAMESPACE::MaterialInstance::SetOffset(size_t offset)
{
	mOffset = static_cast<uint32_t>(offset);
	mShaderParBuffer.Resize(mOffset * mInfo->Stride);

	UpdateShaderParBuffer();
}

void AQUA_NAMESPACE::MaterialInstance::UpdateMaterialInfos(const MAT_NAMESPACE::Material& material, const MaterialInstanceInfo& materialInfo)
{
	const auto& pipeline = **material.GetBasicPipeline();
	const vkLib::PShader& shader = pipeline.GetShader();

	auto updateImage = [&pipeline, &shader](const vkLib::DescriptorLocation& location, 
		vkLib::ImageView imageView, vkLib::Core::Ref<vk::Sampler> sampler)
		{
			vkLib::SampledImageWriteInfo sampledImageInfo{};
			sampledImageInfo.ImageLayout = vk::ImageLayout::eGeneral;
			sampledImageInfo.ImageView = imageView.GetNativeHandle();
			sampledImageInfo.Sampler = *sampler;

			if (!shader.IsEmpty(location.SetIndex, location.Binding))
				pipeline.UpdateDescriptor(location, sampledImageInfo);
		};

	auto updateStorageBuffer = [&shader, &pipeline](
		const vkLib::DescriptorLocation& location, vkLib::Core::BufferResource buffer)
		{
			vkLib::StorageBufferWriteInfo storageBuffer{};
			storageBuffer.Buffer = buffer.BufferHandles->Handle;

			if (!shader.IsEmpty(location.SetIndex, location.Binding) && storageBuffer.Buffer)
				pipeline.UpdateDescriptor(location, storageBuffer);
		};

	auto updateUniformBuffer = [&shader, &pipeline](
		const vkLib::DescriptorLocation& location, vkLib::Core::BufferResource buffer)
		{
			vkLib::UniformBufferWriteInfo uniformBuffer{};
			uniformBuffer.Buffer = buffer.BufferHandles->Handle;

			if (!shader.IsEmpty(location.SetIndex, location.Binding) && uniformBuffer.Buffer)
				pipeline.UpdateDescriptor(location, uniformBuffer);
		};

	for (const auto& [location, resource] : materialInfo.Resources)
	{
		if (resource.Type == GetImageDescType())
			updateImage(location, resource.ImageView, resource.Sampler);
		if (resource.Type == GetStorageBufDescType())
			updateStorageBuffer(location, resource.Buffer);
		if (resource.Type == GetUniformBufDescType())
			updateUniformBuffer(location, resource.Buffer);
	}
}
