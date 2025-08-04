#pragma once
#include "MaterialInstance.h"

AQUA_BEGIN
MAT_BEGIN

// Adding some kind of an import system later
class MaterialAssembler
{
public:
	MaterialAssembler() = default;
	~MaterialAssembler() = default;

	void SetPipelineBuilder(vkLib::PipelineBuilder builder) { mPipelineBuilder = builder; }

	std::expected<MAT_NAMESPACE::Material, vkLib::CompileError> ConstructRayTracingMaterial(const std::string& shader, const vkLib::PreprocessorDirectives& directives = {});
	std::expected<MAT_NAMESPACE::Material, vkLib::CompileError> ConstructDeferGFXMaterial(const std::string& shader, vkLib::GraphicsPipelineConfig config, const vkLib::PreprocessorDirectives& directives = {});

private:
	vkLib::PipelineBuilder mPipelineBuilder;

	static std::string sDeferGFXVertexShader;
};

MAT_END
AQUA_END
