
// version goes here but will be inserted by the user
// So we put a macro as a placeholder for the version
#version 440 core

struct Camera
{
    mat4 Projection;
    mat4 View;
};

struct DirectionalLightSrc
{
    vec3 Color;
    vec3 Direction;
};

struct PointLightSrc
{
    vec3 Position;
    vec3 Intensity;
    vec2 DropRate;
};

struct BSDFInput
{
    vec3 ViewDir;
    vec3 LightDir;
    vec3 Normal;
};

layout(push_constant) uniform ShaderConstants
{
	// Material and ray stuff...
	uint pDirectionalLightCount;
	uint pPointLightCount;
    uint pMaterialRef;
};

layout(set = 0, binding = 0) uniform sampler2D uPositions;
layout(set = 0, binding = 1) uniform sampler2D uNormals;
layout(set = 0, binding = 2) uniform sampler2D uTexCoords;
layout(set = 0, binding = 3) uniform sampler2D uTangents;
layout(set = 0, binding = 4) uniform sampler2D uBitangents;

layout(set = 0, binding = 5) uniform sampler2D uDepthMap[MAX_DEPTH_ARRAY];

layout(std430, set = 1, binding = 0) readonly buffer DirectionalLights
{
    DirectionalLightSrc sDirectionalLights[];
};

layout(std430, set = 1, binding = 1) readonly buffer PointLights
{
    PointLightSrc sPointLights[];
};

layout(std430, set = 1, binding = 2) readonly buffer DepthCameras
{
    Camera uDepthCameras[];
};

layout(std140, set = 1, binding = 3) uniform CameraUniform
{
    Camera uCamera;
};


/* Declaration of shader parameters
* Example:

struct __ShaderParameter
{
    vec3 base_color;
    float roughness;
};

layout(std430, set = 1, binding = 0) readonly buffer __ShaderParameters
{
    __ShaderParameter __sShaderParameters[];
};

* The above piece of code is just one example of how shader parameters could manifest themselves
* for declarations like @vec3.base_color and @float.roughness

*/// Helpler functions...
// Fresnel equations approximation
vec3 FresnelSchlick(float HdotV, in vec3 Reflectivity)
{
    return vec3(Reflectivity) + vec3(1.0 - Reflectivity) * pow(1.0 - HdotV, 5.0);
}

float GeometrySchlickGGX(float DotProduct, float Roughness)
{
    float RoughnessSq = Roughness * Roughness;
    float DotProdClamped = max(DotProduct, SHADING_TOLERANCE);

    return (2.0 * DotProdClamped) / (DotProdClamped + sqrt(RoughnessSq +
        (1 - RoughnessSq) * DotProdClamped * DotProdClamped));
}

// Compute geometry term using Smith's method
float GeometrySmith(float NdotV, float NdotL, float Roughness)
{
    float ggx2 = GeometrySchlickGGX(NdotV, Roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, Roughness);
    return ggx1 * ggx2;
}

// Compute normal distribution function using GGX/Trowbridge-Reitz
float DistributionGGX(float NdotH, float Roughness)
{
    NdotH = max(NdotH, SHADING_TOLERANCE);

    float a2 = Roughness * Roughness;
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = MATH_PI * denom * denom;

    float Value = num / denom;

    const float MaxGGX_Value = 1.0 / (2.0 * TOLERANCE);

    return min(Value, MaxGGX_Value);
}

// Lambertian diffuse BRDF
vec3 LambertianBRDF(in vec3 iNormal, in vec3 LightDir, in vec3 BaseColor)
{
    // Compute the dot product of the surface normal and the light direction
    float NdotL = max(dot(iNormal, LightDir), SHADING_TOLERANCE);

    // Calculate diffuse reflectance using the Lambertian model
    vec3 diffuse = BaseColor / MATH_PI;

    // Multiply by the NdotL term to account for light falloff
    vec3 Lo = diffuse * NdotL;

    return Lo;
}



struct CookTorranceBSDFInput
{
    vec3 ViewDir;
    float Roughness;

    vec3 Normal;
    float Metallic;

    vec3 BaseColor;
    float RefractiveIndex;

	vec3 LightDir;
    float TransmissionWeight;
};

vec3 CookTorranceBRDF(in CookTorranceBSDFInput bsdfInput)
{
	vec3 ViewDir = bsdfInput.ViewDir;
    vec3 iNormal = normalize(bsdfInput.ViewDir + bsdfInput.LightDir);

    float NdotH = max(dot(bsdfInput.Normal, iNormal), SHADING_TOLERANCE);
    float NdotV = max(dot(bsdfInput.Normal, ViewDir), SHADING_TOLERANCE);
    float NdotL = max(dot(bsdfInput.Normal, bsdfInput.LightDir), SHADING_TOLERANCE);
	float HdotV = max(dot(iNormal, ViewDir), SHADING_TOLERANCE);

    vec3 Reflectivity = vec3((bsdfInput.RefractiveIndex - 1.0) /
        (bsdfInput.RefractiveIndex + 1.0));

    Reflectivity *= Reflectivity;

    Reflectivity = mix(Reflectivity, bsdfInput.BaseColor, bsdfInput.Metallic);

    float NDF = DistributionGGX(NdotH, bsdfInput.Roughness);
    float G = GeometrySmith(NdotL, NdotV, bsdfInput.Roughness);
    vec3 F = FresnelSchlick(NdotH, Reflectivity);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL;

    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0 - bsdfInput.TransmissionWeight);
    kD *= kD * (vec3(1.0) - kS);
    kD *= 1.0 - bsdfInput.Metallic;

    vec3 Lo = (kD * bsdfInput.BaseColor / MATH_PI + specular) * NdotL;
    //Lo = specular * NdotL;
    //Lo = (kD * bsdfInput.BaseColor / MATH_PI) * NdotL;

	return Lo;
}

		vec3 Evaluate(BSDFInput bsdfInput)
		{
			CookTorranceBSDFInput cookTorrInput;
			cookTorrInput.ViewDir = bsdfInput.ViewDir;
			cookTorrInput.LightDir = bsdfInput.LightDir;
			cookTorrInput.Normal = bsdfInput.Normal;
			cookTorrInput.Metallic = 0.1;
			cookTorrInput.Roughness = 0.4;
			cookTorrInput.BaseColor = vec3(0.4, 0.0, 0.4);
			cookTorrInput.BaseColor = vec3(1.0, 0.0, 1.0);
			cookTorrInput.RefractiveIndex = 7.5;
			cookTorrInput.TransmissionWeight = 0.0;

			return CookTorranceBRDF(cookTorrInput);
		}
	


// Insert the outputs here...
layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec2 vTexCoords;

float CalcShadow(in vec3 position, float VdotN, uint idx)
{
    const float bias = 0.01;

    vec4 orthoPosition = uDepthCameras[idx].Projection * uDepthCameras[idx].View * vec4(position, 1.0);

    vec2 texCoord = (orthoPosition.xy + vec2(1.0)) * 0.5;

    float occlusionDepth = texture(uDepthMap[idx], texCoord).r;

    // No blurring yet
    if(orthoPosition.z > occlusionDepth + bias * (1.0 - VdotN))
        return 0.0;

    return 1.0;
}

vec3 GetCameraPosition()
{
    mat4 Transposed = transpose(uCamera.View);
    vec3 a = Transposed[0].xyz;
    vec3 b = Transposed[1].xyz;
    vec3 c = Transposed[2].xyz;

    return -(uCamera.View[3].x * a + uCamera.View[3].y * b + uCamera.View[3].z * c);
}

void main()
{
    vec4 PositionValue = texture(uPositions, vTexCoords);
    vec4 NormalValue = texture(uNormals, vTexCoords);
    vec4 TexCoordValue = texture(uTexCoords, vTexCoords);
    vec4 TangentValue = texture(uTangents, vTexCoords);
    vec4 BitangentValue = texture(uBitangents, vTexCoords);

    uvec3 RenderableMetaData = uvec3(PositionValue.w + 0.5, NormalValue.w + 0.5, TexCoordValue.w + 0.5);
    vec3 Position = PositionValue.xyz;
    vec3 Normal = normalize(NormalValue.xyz);
    vec3 Tangent = normalize(TangentValue.xyz);
    vec3 Bitangent = normalize(BitangentValue.xyz);
    vec3 TexCoord = TexCoordValue.xyz;

    BSDFInput bsdfInput;

    bsdfInput.ViewDir = normalize(GetCameraPosition() - Position.xyz);
    bsdfInput.Normal = Normal.xyz;

    float VdotN = normalize(bsdfInput.ViewDir, bsdfInput.Normal);

    FragColor.xyz = vec3(0.0);
    
    for(uint i = 0; i < pDirectionalLightCount; i++)
    {
        bsdfInput.LightDir = -normalize(sDirectionalLights[i].Direction);
        FragColor.xyz += Evaluate(bsdfInput) * CalcShadow(Position, VdotN, i);
        //FragColor.xyz = normalize(GetCameraPosition() - Position.xyz);
    }

    for(uint i = 0; i < pPointLightCount; i++)
    {
        vec3 LightDisplace = sPointLights[i].Position - Position.xyz;

        bsdfInput.LightDir = -normalize(LightDisplace);

        float Distance = length(LightDisplace);
        float Attenuation = Distance * (Distance * sPointLights[i].DropRate.x + sPointLights[i].DropRate.y);

        FragColor.xyz += Evaluate(bsdfInput) / Attenuation;
    }
}

        FragColor.xyz += Evaluate(bsdfInput) / Attenuation;
    }
}

