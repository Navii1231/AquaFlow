#pragma once
#include "../../Core/AqCore.h"
#include "../../Core/SharedRef.h"
#include "../Pipelines/PipelineConfig.h"

AQUA_BEGIN

enum class RenderingFeature : uint64_t
{
	eShadow            = 1,
	eSSAO              = 2,
	eBloomEffect       = 4,
	eMotionBlur        = 8,
};

enum class PostProcessing : uint32_t
{
	eToneMap           = 1,
	eBW                = 2,
	eSharpen           = 4,
	eBlur              = 8,
	eInvert            = 16,
};

enum class RenderingStage
{
	eFrontEnd          = 1,
	eShading           = 2,
	eBackEnd           = 3,
};

enum class SurfaceType
{
	eNone              = 1,
	ePoint             = 2,
	eLine              = 3,
	eModel             = 4,
};

using RendererFeatureFlags = vk::Flags<RenderingFeature>;
using PostProcessingFlags = vk::Flags<PostProcessing>;

struct FeaturesEnabled
{
	alignas(4) uint32_t SSAO = 0;
	alignas(4) uint32_t ShadowMapping = 0;
	alignas(4) uint32_t BloomEffect = 0;
};

struct DirectionalLightInfo
{
	DirectionalLightSrc SrcInfo;
	glm::vec3 CubeSize;
	glm::vec3 Position;
	mutable uint32_t Offset = 0;
};

struct PointLightInfo
{
	PointLightSrc SrcInfo;
	mutable uint32_t Offset = 0;
};

using DirectionalLightList = std::vector<DirectionalLightSrc>;
using PointLightList = std::vector<PointLightSrc>;

AQUA_END
