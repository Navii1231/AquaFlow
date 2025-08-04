#pragma once
#include "MaterialConfig.h"
#include "../Execution/Graph.h"

AQUA_BEGIN

struct MaterialInstanceInfo
{
	MAT_NAMESPACE::ResourceMap Resources;
	MAT_NAMESPACE::ShaderParameterSet ShaderParameters;
	vkLib::DescriptorLocation ParameterLocation;
	size_t Stride = 0;
	MAT_NAMESPACE::Platform RendererType;
};

using MaterialInstanceInfoRef = SharedRef<MaterialInstanceInfo>;

// To ease ourselves at setting up the resources
// #set 0 and #set 1 are reserved across all shading platforms
// so assign anything to them at your own risk

/*
* #MaterialInstance is a thin wrapper over Exec::#Operation or rather raw #Material
* This class introduces new functionalities and completes the #Material conception.
* It's only through this class that the user will interact with the core material logic,
* configure shader parameters and send the required material resources to the GPU
* The data in the uniform buffer will be transferred with each #MaterialInstance assignment
*/ 
class MaterialInstance
{
public:
	MaterialInstance() = default;
	virtual ~MaterialInstance() = default;

	template <typename Fn>
	static void TraverseImageResources(MAT_NAMESPACE::ResourceMap& resources,Fn&& fn);

	template <typename Fn>
	static void TraverseStorageBuffers(MAT_NAMESPACE::ResourceMap& resources,Fn&& fn);

	template <typename Fn>
	static void TraverseUniformBuffers(MAT_NAMESPACE::ResourceMap& resources,Fn&& fn);

	virtual const vkLib::BasicPipeline* GetBasicPipeline() const { return *mCoreMaterial.GetBasicPipeline(); }

	virtual void UpdateDescriptors() const { UpdateMaterialInfos(mCoreMaterial, *mInfo); }

	// you can send only have predefine #GLSL/#HLSL types
	template <typename T>
	void SetShaderParameter(const std::string& name, T&& parVal);
	void UpdateShaderParBuffer();

	void SetOffset(size_t offset);

	uint32_t GetOffset() const { return mOffset; }
	MaterialInstanceInfoRef GetInfo() const { return mInfo; }
	MAT_NAMESPACE::Material GetMaterial() const { return mCoreMaterial; }

	// set 0 and 1 are reserved, so assign them at your own risk
	MAT_NAMESPACE::Resource& operator[](const vkLib::DescriptorLocation& location) { return mInfo->Resources[location]; }
	const MAT_NAMESPACE::Resource& operator[](const vkLib::DescriptorLocation& location) const { return mInfo->Resources.at(location); }

	consteval static vk::DescriptorType GetImageDescType() { return vk::DescriptorType::eSampledImage; }
	consteval static vk::DescriptorType GetStorageBufDescType() { return vk::DescriptorType::eStorageBuffer; }
	consteval static vk::DescriptorType GetUniformBufDescType() { return vk::DescriptorType::eUniformBuffer; }

	static void UpdateMaterialInfos(const MAT_NAMESPACE::Material& material, const MaterialInstanceInfo& materialInfo);

protected:
	MaterialInstanceInfoRef mInfo;
	MAT_NAMESPACE::Material mCoreMaterial;
	vkLib::GenericBuffer mShaderParBuffer;
	uint32_t mOffset = 0;
	uint32_t mInstanceID = 0; // Not yet being used

	friend class MaterialBuilder;
};

AQUA_END

template <typename Func>
void AQUA_NAMESPACE::MaterialInstance::TraverseImageResources(MAT_NAMESPACE::ResourceMap& resources, Func&& fn)
{
	for (auto& [loc, resource] : resources)
	{
		if (resource.Type == GetImageDescType())
			fn(loc, resource.ImageView);
	}
}

template <typename Func>
void AQUA_NAMESPACE::MaterialInstance::TraverseStorageBuffers(MAT_NAMESPACE::ResourceMap& resources, Func&& fn)
{
	for (auto& [loc, resource] : resources)
	{
		if (resource.Type == GetStorageBufDescType())
			fn(loc , resource.Buffer);
	}
}

template <typename Func>
void AQUA_NAMESPACE::MaterialInstance::TraverseUniformBuffers(MAT_NAMESPACE::ResourceMap& resources, Func&& fn)
{
	for (auto& [loc, resource] : resources)
	{
		if (resource.Type == GetUniformBufDescType())
			fn(loc, resource.Buffer);
	}
}

template <typename T>
void AQUA_NAMESPACE::MaterialInstance::SetShaderParameter(const std::string& name, T&& parVal)
{
	if (mInfo->ShaderParameters[name].TypeSize != sizeof(parVal))
		return;

	auto memory = (T*)mShaderParBuffer.MapMemory<uint8_t>(mInfo->ShaderParameters[name].TypeSize,
		mInfo->ShaderParameters[name].Offset + mOffset * mInfo->Stride);

	*memory = parVal;

	mShaderParBuffer.UnmapMemory();
}
