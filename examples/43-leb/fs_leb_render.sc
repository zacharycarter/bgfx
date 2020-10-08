$input v_texcoord0, v_wpos

#include "leb_common.sh"

SAMPLER2D(u_DmapSampler, 0);

void main()
{
    float filterSize = 1.0f / float(textureSize(u_DmapSampler, 0).x);// sqrt(dot(dFdx(texCoord), dFdy(texCoord)));
    float sx0 = texture2DLod(u_DmapSampler, v_texcoord0 - vec2(filterSize, 0.0), 0.0).r;
    float sx1 = texture2DLod(u_DmapSampler, v_texcoord0 + vec2(filterSize, 0.0), 0.0).r;
    float sy0 = texture2DLod(u_DmapSampler, v_texcoord0 - vec2(0.0, filterSize), 0.0).r;
    float sy1 = texture2DLod(u_DmapSampler, v_texcoord0 + vec2(0.0, filterSize), 0.0).r;
    float sx = sx1 - sx0;
    float sy = sy1 - sy0;

    vec3 n = normalize(vec3(u_DmapFactor * 0.03 / filterSize * 0.5f * vec2(-sx, -sy), 1));
    
    vec3 wi = normalize(vec3(1, 1, 1));
    float d = dot(wi, n) * 0.5 + 0.5;
    vec3 albedo = vec3(252, 197, 150) / 255.0f;
	
    vec3 shading = (d / 3.14159) * albedo;
    
    gl_FragColor = vec4(shading, 1);
}
