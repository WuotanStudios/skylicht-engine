Texture2D uTexDiffuse : register(t0);
SamplerState uTexDiffuseSampler : register(s0);

Texture2D uTexRamp : register(t1);
SamplerState uTexRampSampler : register(s1);

#if defined(SHADOW)
Texture2DArray uShadowMap : register(t2);
SamplerState uShadowMapSampler : register(s2);
#endif

struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float2 tex0 : TEXCOORD0;
	float3 worldNormal: WORLDNORMAL;
	float3 worldViewDir: WORLDVIEWDIR;
	float3 worldPos: WORLDPOSITION;
	float3 depth: DEPTH;
};

cbuffer cbPerFrame
{
	float4 uLightDirection;
	float4 uLightColor;
	float4 uColor;
	float4 uShadowColor;
	float2 uWrapFactor;
	float3 uSpecular;
#if defined(SHADOW)
	float3 uShadowDistance;
	float4x4 uShadowMatrix[3];
#endif
};

#include "../../PostProcessing/HLSL/LibToneMapping.hlsl"

#if defined(SHADOW)
#include "../../Shadow/HLSL/LibShadow.hlsl"
#endif

float4 main(PS_INPUT input) : SV_TARGET
{
	float3 diffuseMap = sRGB(uTexDiffuse.Sample(uTexDiffuseSampler, input.tex0).rgb);
	float3 color = sRGB(uColor.rgb);
	float3 shadowColor = sRGB(uShadowColor.rgb);
	float3 lightColor = sRGB(uLightColor.rgb);
	
	float visibility = 1.0;
	
#if defined(SHADOW)
	// shadow
	float depth = length(input.depth);

	float4 shadowCoord[3];
	shadowCoord[0] = mul(float4(input.worldPos, 1.0), uShadowMatrix[0]);
	shadowCoord[1] = mul(float4(input.worldPos, 1.0), uShadowMatrix[1]);
	shadowCoord[2] = mul(float4(input.worldPos, 1.0), uShadowMatrix[2]);

	float shadowDistance[3];
	shadowDistance[0] = uShadowDistance.x;
	shadowDistance[1] = uShadowDistance.y;
	shadowDistance[2] = uShadowDistance.z;
	visibility = shadow(shadowCoord, shadowDistance, depth);
#endif

	float NdotL = max((dot(input.worldNormal, uLightDirection.xyz) + uWrapFactor.x) / (1.0 + uWrapFactor.x), 0.0);
	float3 rampMap = uTexRamp.Sample(uTexRampSampler, float2(NdotL, NdotL)).rgb;

	// Shadows intensity through alpha
	float3 ramp = lerp(color, shadowColor, uColor.a * (1.0 - visibility));
	ramp = lerp(ramp, color, rampMap);
	
	// Specular
	float3 h = normalize(uLightDirection.xyz + input.worldViewDir);
	float NdotH = max(0, dot(input.worldNormal, h));
	
	float spec = pow(NdotH, uSpecular.x*128.0) * uSpecular.y;
	spec = smoothstep(0.5-uSpecular.z*0.5, 0.5+uSpecular.z*0.5, spec);
	
	return float4(diffuseMap * lightColor * ramp * (0.5 + visibility * 0.5) + lightColor * spec * visibility, 1.0);
}