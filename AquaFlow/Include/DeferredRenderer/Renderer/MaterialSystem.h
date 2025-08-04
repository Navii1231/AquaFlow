#pragma once
#include "RendererConfig.h"
#include "MaterialConfig.h"

#include "../../Material/MaterialInstance.h"
#include "../Renderable/RenderTargetFactory.h"

AQUA_BEGIN

struct MaterialSystemConfig;

// We need material caching...
// we need to reserve primitive materials for objects line, points and bezier curves
class MaterialSystem
{
public:
	MaterialSystem();
	~MaterialSystem() = default;

	void SetShaderDirectory(const std::string& shaders);

	void SetHyperSurfaceRenderFactory(RenderTargetFactory factory);
	void SetCtx(vkLib::Context ctx);

	// builds and caches a material one
	MaterialInstance BuildInstance(const std::string& name,
		const DeferGFXMaterialCreateInfo& createInfo) const;

	// finds the material in the cache
	std::expected<MaterialInstance, bool> operator[](const std::string& name) const;

private:
	SharedRef<MaterialSystemConfig> mConfig;

private:
	void SetupDefaultRenderFactory();

	void InitInstance(MaterialTemplate& temp, MaterialInstance& instance) const;

	SharedRef<MaterialTemplate> FindTemplate(const std::string& name) const;
	SharedRef<MaterialTemplate> CreateTemplate(const std::string& name, 
		const DeferGFXMaterialCreateInfo& createInfo, const MaterialInstance& clone) const;

	void BuildSparePipelines();

};

AQUA_END
