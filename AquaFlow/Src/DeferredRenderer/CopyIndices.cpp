#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderable/CopyIndices.h"

void AQUA_NAMESPACE::CopyIdxPipeline::UpdateDescriptors(vkLib::GenericBuffer dst, vkLib::GenericBuffer src)
{
	vkLib::StorageBufferWriteInfo idx{};
	idx.Buffer = src.GetNativeHandles().Handle;

	this->UpdateDescriptor({ 0, 0, 0 }, idx);

	idx.Buffer = dst.GetNativeHandles().Handle;

	this->UpdateDescriptor({ 0, 1, 0 }, idx);
}

void AQUA_NAMESPACE::CopyIdxPipeline::operator()(vk::CommandBuffer cmd, vkLib::GenericBuffer dst,
	vkLib::GenericBuffer src, uint32_t vertexCount)
{
	size_t idxCount = src.GetSize() / sizeof(uint32_t);
	size_t idxOffset = dst.GetSize() / sizeof(uint32_t);

	glm::uvec3 workGrps = glm::uvec3(idxCount / GetWorkGroupSize().x + 1, 1, 1);

	dst.Resize(dst.GetSize() + src.GetSize());

	UpdateDescriptors(dst, src);

	Begin(cmd);

	Activate();
	SetShaderConstant("eCompute.ShaderConstants.Index_0", static_cast<uint32_t>(vertexCount));
	SetShaderConstant("eCompute.ShaderConstants.Index_1", static_cast<uint32_t>(idxOffset));
	SetShaderConstant("eCompute.ShaderConstants.Index_2", static_cast<uint32_t>(idxCount));

	Dispatch(workGrps);

	End();
}
