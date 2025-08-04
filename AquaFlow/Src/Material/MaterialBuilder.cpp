#include "Core/Aqpch.h"
#include "MaterialSystem/MaterialBuilder.h"

std::unordered_map<std::string, uint32_t> AQUA_NAMESPACE::MaterialBuilder::sGLSLTypeToSize =
{
	{"int", 4}, {"uint", 4}, {"float", 4},
	{"vec2", 8}, {"ivec2", 8}, {"uvec2", 8},
	{"vec3", 12}, {"ivec3", 12}, {"uvec3", 12},
	{"vec4", 16}, {"ivec4", 16}, {"uvec4", 16},
	{"mat2", 16}, {"umat2", 16}, {"imat2", 16},
	{"mat3", 24}, {"umat3", 24}, {"imat3", 24},
	{"mat4", 32}, {"umat4", 32}, {"imat4", 32},
};

AQUA_NAMESPACE::MaterialInstance AQUA_NAMESPACE::MaterialBuilder::BuildRTInstance(const RTMaterialCreateInfo& createInfo)
{
	MaterialPostprocessor postProcessor{};
	postProcessor.SetShaderTextView(createInfo.ShaderCode);
	postProcessor.SetBasicTypeNames(GetGLSLBasicTypes());
	postProcessor.SetCustomPrefix("sCPU_CustomParameters.");
	postProcessor.SetParameterIdentifier('@');
	postProcessor.SetShaderModules(mImports);

	MAT_NAMESPACE::ShaderParameterSet set;

	auto text1 = postProcessor.ResolveCustomParameters(set);
	postProcessor.SetShaderTextView(*text1);
	auto glslCode = postProcessor.ResolveImportDirectives();

	vkLib::PreprocessorDirectives directives;
	directives["SHADING_TOLERANCE"] = std::to_string(createInfo.ShadingTolerance);
	directives["EPSILON"] = std::to_string(std::numeric_limits<float>::epsilon());
	directives["POWER_HEURISTICS_EXP"] = std::to_string(createInfo.PowerHeuristics);
	directives["WORKGROUP_SIZE"] = std::to_string(createInfo.WorkGroupSize);
	directives["SHADER_PARS_SET_IDX"] = std::to_string(mMaterialSetBinding.SetIndex);
	directives["SHADER_PARS_BINDING_IDX"] = std::to_string(mMaterialSetBinding.Binding);

	directives["EMPTY_MATERIAL_ID"] = std::to_string(createInfo.EmptyMaterialID);
	directives["SKYBOX_MATERIAL_ID"] = std::to_string(createInfo.SkyboxMaterialID);
	directives["LIGHT_MATERIAL_ID"] = std::to_string(createInfo.LightMaterialID);
	directives["RR_CUTOFF_CONST"] = std::to_string(createInfo.RussianRouletteCutoffConst);

	std::string pipelineCode = std::string(mFrontEnd);
	pipelineCode += "\n\n";
	pipelineCode += *glslCode;
	pipelineCode += "\n\n";
	pipelineCode += std::string(mBackEnd);

	auto material = mMaterialAsembler.ConstructRayTracingMaterial(pipelineCode, directives);

	MaterialInstance instance{};
	InitializeInstance(instance, *material, set);

	return instance;
}

AQUA_NAMESPACE::MaterialInstance AQUA_NAMESPACE::MaterialBuilder::BuildDeferGFXInstance(const DeferGFXMaterialCreateInfo& createInfo)
{
	MaterialPostprocessor postProcessor{};
	postProcessor.SetShaderTextView(createInfo.ShaderCode);
	postProcessor.SetBasicTypeNames(GetGLSLBasicTypes());
	postProcessor.SetCustomPrefix("sCPU_CustomParameters.");
	postProcessor.SetParameterIdentifier('@');
	postProcessor.SetShaderModules(mImports);

	MAT_NAMESPACE::ShaderParameterSet set;

	auto text1 = postProcessor.ResolveCustomParameters(set);
	postProcessor.SetShaderTextView(*text1);
	auto glslCode = postProcessor.ResolveImportDirectives();

	vkLib::PreprocessorDirectives directives;
	directives["SHADING_TOLERANCE"] = std::to_string(createInfo.ShadingTolerance);
	directives["MAX_DEPTH_ARRAY"] = std::to_string(1);
	directives["SHADER_PARS_SET_IDX"] = std::to_string(mMaterialSetBinding.SetIndex);
	directives["SHADER_PARS_BINDING_IDX"] = std::to_string(mMaterialSetBinding.Binding);
	directives["MATH_PI"] = std::to_string(glm::pi<float>());
	directives["TOLERANCE"] = std::to_string(createInfo.ShadingTolerance);

	directives["EMPTY_MATERIAL_ID"] = std::to_string(createInfo.EmptyMaterialID);

	std::string pipelineCode = std::string(mFrontEnd);
	pipelineCode += "\n\n";
	pipelineCode += *glslCode;
	pipelineCode += "\n\n";
	pipelineCode += std::string(mBackEnd);

	auto material = mMaterialAsembler.ConstructDeferGFXMaterial(pipelineCode, createInfo.GFXConfig, directives);

	MaterialInstance instance{};
	InitializeInstance(instance, *material, set);

	return instance;
}

void AQUA_NAMESPACE::MaterialBuilder::InitializeInstance(MaterialInstance& instance, MAT_NAMESPACE::Material material, MAT_NAMESPACE::ShaderParameterSet& set)
{
	instance.mCoreMaterial = material;

	instance.mInfo = std::make_shared<MaterialInstanceInfo>();
	instance.mInfo->ParameterLocation = mMaterialSetBinding;
	instance.mInfo->RendererType = MAT_NAMESPACE::Platform::ePathTracer;
	instance.mInfo->Resources = {}; // starts at being empty

	RetrieveParameterSpecs(instance.mInfo->Stride, set);

	instance.mInfo->ShaderParameters = set;

	instance.mShaderParBuffer = mResourcePool.CreateGenericBuffer(
		vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);

	instance.mInstanceID = 0; // Not yet being used by any rendering system

	instance.SetOffset(0);
}

std::vector<std::string> AQUA_NAMESPACE::MaterialBuilder::GetGLSLBasicTypes()
{
	std::vector<std::string> types(sGLSLTypeToSize.size());

	for (const auto& [name, size] : sGLSLTypeToSize)
		types.emplace_back(name);

	return types;
}

void AQUA_NAMESPACE::MaterialBuilder::RetrieveParameterSpecs(size_t& stride, MAT_NAMESPACE::ShaderParameterSet& set)
{
	stride = 0;

	for (auto& [name, parInfo] : set)
	{
		parInfo.TypeSize = sGLSLTypeToSize.at(parInfo.Type);
		parInfo.Offset = stride;

		stride += parInfo.TypeSize;
	}
}
