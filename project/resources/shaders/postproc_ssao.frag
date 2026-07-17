#version 450 core
// Screen-Space Ambient Occlusion, reconstructing view-space position and normal
// from the scene depth buffer (no G-buffer needed -- keeps the forward shaders
// untouched). Outputs a single-channel occlusion factor in [0,1] (1 = fully
// unoccluded / bright, 0 = fully occluded / dark).
in vec2 TexCoord;
out float FragColor;

uniform sampler2D depthTexture;
uniform mat4 projection;
uniform mat4 invProjection;
uniform float radius;
uniform float intensity;
uniform vec2 noiseScale; // screenSize / noiseTexSize, tiles the rotation noise

// A small set of hemisphere sample offsets (tangent space). Kept as a constant
// kernel so no extra uniform upload is needed.
const int SAMPLE_COUNT = 16;
const vec3 kernel[16] = vec3[16](
    vec3( 0.0492,  0.0330,  0.0585), vec3(-0.0400,  0.0200,  0.0900),
    vec3( 0.0300, -0.0450,  0.0700), vec3(-0.0600, -0.0300,  0.0500),
    vec3( 0.0700,  0.0600,  0.1200), vec3(-0.0900,  0.0500,  0.1500),
    vec3( 0.0200, -0.0800,  0.1000), vec3(-0.0300, -0.0900,  0.1800),
    vec3( 0.1200,  0.1000,  0.2000), vec3(-0.1500,  0.0800,  0.2200),
    vec3( 0.0500, -0.1400,  0.2500), vec3(-0.0700, -0.1600,  0.3000),
    vec3( 0.2000,  0.1800,  0.3500), vec3(-0.2500,  0.1500,  0.4000),
    vec3( 0.1000, -0.2200,  0.4500), vec3(-0.1200, -0.2600,  0.5000)
);

// Reconstruct view-space position from a UV + sampled depth.
vec3 viewPosFromDepth(vec2 uv, float depth)
{
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = invProjection * clip;
    return view.xyz / view.w;
}

// A cheap per-pixel pseudo-random value to rotate the kernel (hides banding).
float rand(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    float depth = texture(depthTexture, TexCoord).r;
    // Skybox / cleared background (depth == 1.0): no occlusion.
    if (depth >= 1.0)
    {
        FragColor = 1.0;
        return;
    }

    vec3 fragPos = viewPosFromDepth(TexCoord, depth);

    // Reconstruct the normal from the derivatives of the reconstructed position
    // (screen-space). Good enough without a dedicated normal buffer.
    vec3 normal = normalize(cross(dFdx(fragPos), dFdy(fragPos)));

    // Random rotation vector per pixel for kernel jitter.
    float r = rand(TexCoord * noiseScale) * 6.2831853;
    vec3 randomVec = vec3(cos(r), sin(r), 0.0);

    // Build a TBN basis to orient the hemisphere along the surface normal.
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        vec3 samplePos = fragPos + (TBN * kernel[i]) * radius;

        // Project the sample back to screen space to look up its stored depth.
        vec4 offset = projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0)
            continue;

        float sampleDepth = texture(depthTexture, offset.xy).r;
        vec3 sampleSurface = viewPosFromDepth(offset.xy, sampleDepth);

        // If real geometry at that pixel is closer to the camera than our sample
        // point, the sample is occluded. Range check avoids halos across big
        // depth discontinuities.
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleSurface.z + 0.0001));
        if (sampleSurface.z >= samplePos.z + 0.02)
            occlusion += rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(SAMPLE_COUNT)) * intensity;
    FragColor = clamp(occlusion, 0.0, 1.0);
}
