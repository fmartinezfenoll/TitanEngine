#version 450 core
// FXAA (Fast Approximate Anti-Aliasing), based on Timothy Lottes' algorithm.
// Detects edges from local luminance contrast and blends along them. Operates
// on the final LDR image, independent of geometry.
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D image;

const float EDGE_MIN = 1.0 / 128.0;
const float EDGE_MAX = 1.0 / 8.0;
const float SPAN_MAX = 8.0;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(image, 0));

    vec3 rgbM  = texture(image, TexCoord).rgb;
    vec3 rgbNW = texture(image, TexCoord + vec2(-1.0, -1.0) * texelSize).rgb;
    vec3 rgbNE = texture(image, TexCoord + vec2( 1.0, -1.0) * texelSize).rgb;
    vec3 rgbSW = texture(image, TexCoord + vec2(-1.0,  1.0) * texelSize).rgb;
    vec3 rgbSE = texture(image, TexCoord + vec2( 1.0,  1.0) * texelSize).rgb;

    float lumaM  = luma(rgbM);
    float lumaNW = luma(rgbNW);
    float lumaNE = luma(rgbNE);
    float lumaSW = luma(rgbSW);
    float lumaSE = luma(rgbSE);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    // Skip pixels with little local contrast (no visible edge there).
    if (lumaMax - lumaMin < max(EDGE_MIN, lumaMax * EDGE_MAX))
    {
        FragColor = vec4(rgbM, 1.0);
        return;
    }

    // Edge direction from the luminance gradient of the 4 corners.
    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * EDGE_MAX, EDGE_MIN);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-SPAN_MAX), vec2(SPAN_MAX)) * texelSize;

    // Sample along the edge and blend.
    vec3 rgbA = 0.5 * (
        texture(image, TexCoord + dir * (1.0 / 3.0 - 0.5)).rgb +
        texture(image, TexCoord + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(image, TexCoord + dir * -0.5).rgb +
        texture(image, TexCoord + dir *  0.5).rgb);

    float lumaB = luma(rgbB);
    if (lumaB < lumaMin || lumaB > lumaMax)
        FragColor = vec4(rgbA, 1.0);
    else
        FragColor = vec4(rgbB, 1.0);
}
