// File Generated by Assets/BuildShader.py - source: [SGPointLightShadowFS.d.glsl : _]
precision highp float;
precision highp sampler2D;
precision highp sampler2DArray;
uniform sampler2D uTexPosition;
uniform sampler2D uTexNormal;
uniform sampler2D uTexData;
uniform samplerCube uShadowMap;
uniform vec4 uCameraPosition;
uniform vec4 uLightPosition;
uniform vec4 uLightAttenuation;
uniform vec4 uLightColor;
in vec2 varTexCoord0;
out vec4 FragColor;
void main(void)
{
	vec3 position = texture(uTexPosition, varTexCoord0.xy).xyz;
	vec3 normal = texture(uTexNormal, varTexCoord0.xy).xyz;
	vec3 data = texture(uTexData, varTexCoord0.xy).rgb;
	vec3 v = uCameraPosition.xyz - position;
	vec3 viewDir = normalize(v);
	float spec = data.r;
	float gloss = data.g;
	vec3 direction = uLightPosition.xyz - position;
	float distance = length(direction);
	float attenuation = max(0.0, 1.0 - (distance * uLightAttenuation.y)) * uLightColor.a;
	vec3 lightDir = normalize(direction);
	float NdotL = max(0.0, dot(lightDir, normal));
	vec3 H = normalize(direction + viewDir);
	float NdotE = max(0.0, dot(normal, H));
	float specular = pow(NdotE, 100.0f * gloss) * spec;
	float bias = 0.2;
	float d = distance - bias;
	float shadow = 0.0;
	float samples = 2.0;
	float offset = 0.01;
	float delta = offset / (samples * 0.5);
	vec3 fragToLight;
	float x = -offset;
	float y = -offset;
	float z = -offset;
	{fragToLight = -lightDir + vec3(x, y, z);shadow += step(texture(uShadowMap, fragToLight).r, d);};
	z += delta;
	{fragToLight = -lightDir + vec3(x, y, z);shadow += step(texture(uShadowMap, fragToLight).r, d);};
	z = -offset;
	y += delta;
	{fragToLight = -lightDir + vec3(x, y, z);shadow += step(texture(uShadowMap, fragToLight).r, d);};
	z += delta;
	{fragToLight = -lightDir + vec3(x, y, z);shadow += step(texture(uShadowMap, fragToLight).r, d);};
	x += delta;
	y = -offset;
	z = -offset;
	{fragToLight = -lightDir + vec3(x, y, z);shadow += step(texture(uShadowMap, fragToLight).r, d);};
	z += delta;
	{fragToLight = -lightDir + vec3(x, y, z);shadow += step(texture(uShadowMap, fragToLight).r, d);};
	z = -offset;
	y += delta;
	{fragToLight = -lightDir + vec3(x, y, z);shadow += step(texture(uShadowMap, fragToLight).r, d);};
	z += delta;
	{fragToLight = -lightDir + vec3(x, y, z);shadow += step(texture(uShadowMap, fragToLight).r, d);};
	shadow /= (samples * samples * samples);
	shadow = max(0.0, 1.0 - shadow);
	vec3 lightColor = NdotL * attenuation * uLightColor.rgb * shadow;
	FragColor = vec4(lightColor, specular * attenuation * shadow);
}
