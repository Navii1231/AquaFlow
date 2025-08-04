#pragma once
#include "MaterialAssembler.h"
#include "MaterialInstance.h"
#include "MaterialPostprocessor.h"

AQUA_BEGIN

struct RTMaterialCreateInfo
{
	std::string ShaderCode;
	uint32_t WorkGroupSize = 256;
	float ShadingTolerance = 0.01f;
	float PowerHeuristics = 2.0f;

	int EmptyMaterialID = -1;
	int SkyboxMaterialID = -2;
	int LightMaterialID = -3;
	int RussianRouletteCutoffConst = -4;
};

struct DeferGFXMaterialCreateInfo
{
	std::string ShaderCode;
	float ShadingTolerance = 0.01f;

	vkLib::GraphicsPipelineConfig GFXConfig;

	int EmptyMaterialID = -1;
};

class MaterialBuilder
{
public:
	MaterialBuilder() = default;
	~MaterialBuilder() = default;

	void SetAssembler(MAT_NAMESPACE::MaterialAssembler assembler) { mMaterialAsembler = assembler; }
	void SetResourcePool(vkLib::ResourcePool pool) { mResourcePool = pool; }

	void SetFrontEndView(std::string_view frontEnd) { mFrontEnd = frontEnd; }
	void SetBackEndView(std::string_view backEnd) { mBackEnd = backEnd; }

	void SetImports(const MAT_NAMESPACE::ShaderImportMap& imports) { mImports = imports; }
	void AddImport(const std::string& importName, const std::string shaderModule) { mImports[importName] = shaderModule; }

	MaterialInstance BuildRTInstance(const RTMaterialCreateInfo& createInfo);
	MaterialInstance BuildDeferGFXInstance(const DeferGFXMaterialCreateInfo& createInfo);

	void InitializeInstance(MaterialInstance& instance, MAT_NAMESPACE::Material material, MAT_NAMESPACE::ShaderParameterSet& set);
	void RegisterMaterial(const std::string& name, MaterialInstance instance) 
	{ mMaterialCache[name] = instance; }

	static std::vector<std::string> GetGLSLBasicTypes();

	MaterialInstance operator[](const std::string& name) const { return mMaterialCache.at(name); }

private:
	std::unordered_map<std::string, MaterialInstance> mMaterialCache;
	MAT_NAMESPACE::ShaderImportMap mImports;
	MAT_NAMESPACE::MaterialAssembler mMaterialAsembler;

	std::string_view mFrontEnd;
	std::string_view mBackEnd;

	vkLib::ResourcePool mResourcePool;
	vkLib::DescriptorLocation mMaterialSetBinding;

	static std::unordered_map<std::string, uint32_t> sGLSLTypeToSize;

private:
	void RetrieveParameterSpecs(size_t& Stride, MAT_NAMESPACE::ShaderParameterSet& set);
};

AQUA_END
