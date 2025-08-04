#pragma once
#include "Core.h"
#include "Wavefront/BVHFactory.h"
#include "RayTracingStructures.h"

#include "Geometry3D/Geometry.h"
#include "Geometry3D/MeshLoader.h"

PH_BEGIN

struct EstimatorComputePipeline : public vkLib::ComputePipeline
{
public:
	EstimatorComputePipeline() = default;
	EstimatorComputePipeline(const vkLib::PShader& shader, vkLib::ResourcePool manager);

	void UpdateDescriptors();
	void UpdateCameraPosition(float deltaTime, float speed, CameraMovementFlags movement);
	void UpdateCameraOrientation(float deltaTime, float speed, float xpos, float ypos, bool Reset);

	void SubmitRenderable(const AquaFlow::MeshData& meshData, const Material& color,
		RenderableType type, uint32_t bvhDepth);

public:
	// Set 0...
	EstimatorTarget mTarget;
	vk::ImageLayout mBufferLayout = vk::ImageLayout::eGeneral;

	uint32_t mResetBounceLimit = 2;
	uint32_t mResetSampleCount = 1;

	CameraData mCameraData;
	SceneInfo mSceneData;

	// Set 1...
	vkLib::Buffer<Camera> mCamera;

	GeometryBuffers mGeomBuffersShared;
	GeometryBuffers mGeomBuffersLocal;

	MaterialBuffer mMaterials; // Temporary... will be removed once a runtime material evaluation is implemented!
	LightPropsBuffer mLightProps;

	vkLib::Buffer<MeshInfo> mMeshInfos;
	vkLib::Buffer<LightInfo> mLightInfos;

	vkLib::Buffer<SceneInfo> mSceneInfo;

	// Set 2...
	vkLib::Buffer<uint32_t> mCPU_Feedback;

	vkLib::Image mSkybox;
	vkLib::Core::Ref<vk::Sampler> mSkyboxSampler;

	GLFWwindow* mWindow = nullptr;

private:
	vkLib::Image LoadImage(const std::string& filepath, vkLib::ResourcePool manager) const;

	GeometryBuffers CreateGeometryBuffers(vk::MemoryPropertyFlags memProps, vkLib::ResourcePool manager);

	template <typename T, typename Iter, typename Fn>
	void CopyVertexAttrib(vkLib::Buffer<T>& SharedBuffer, vkLib::Buffer<T>& LocalBuffer, 
		Iter Begin, Iter End, Fn CopyRoutine);
};

template<typename T, typename Iter, typename Fn>
void EstimatorComputePipeline::CopyVertexAttrib(vkLib::Buffer<T>& SharedBuffer, 
	vkLib::Buffer<T>& LocalBuffer, Iter Begin, Iter End, Fn CopyRoutine)
{
	size_t LocalBufferSize = LocalBuffer.GetSize();
	size_t HostCount = End - Begin;

	if (HostCount == 0)
		return;

	SharedBuffer.Clear();
	SharedBuffer.Resize(HostCount);

	T* BeginDevice = SharedBuffer.MapMemory(HostCount);
	T* EndDevice = BeginDevice + HostCount;

	CopyRoutine(BeginDevice, EndDevice, &(*Begin), &(*(End - 1)) + 1);

	SharedBuffer.UnmapMemory();

	LocalBuffer.Resize(LocalBufferSize + HostCount);

	vk::BufferCopy CopyInfo{};
	CopyInfo.setSrcOffset(0);
	CopyInfo.setDstOffset(LocalBufferSize);
	CopyInfo.setSize(HostCount);

	vkLib::CopyBufferRegions(LocalBuffer, SharedBuffer, std::vector<vk::BufferCopy>({ CopyInfo }));
}

PH_END
