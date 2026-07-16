#version 450 core
in vec3 FragNormal;
in vec2 FragTexCoord;
in vec3 WorldPos;
in mat3 TBN;
out vec4 FragColor;

uniform vec4 baseColor;      // hologram tint (default cyan-ish)
uniform vec3 cameraWorldPos;
uniform float time;

// Look constants.
const float SCANLINE_DENSITY = 60.0; // number of horizontal bands over the model
const float SCANLINE_SPEED = 2.5;    // how fast the bands scroll upward
const float FRESNEL_POWER = 2.5;     // tightness of the bright rim
const float FLICKER_SPEED = 12.0;    // subtle brightness flicker rate
const float BASE_ALPHA = 0.45;       // overall translucency

void main()
{
    vec3 N = normalize(FragNormal);
    vec3 V = normalize(cameraWorldPos - WorldPos);

    // Fresnel rim: edges facing away from the camera glow brightest -- the
    // characteristic "solid outline, hollow center" hologram look.
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), FRESNEL_POWER);

    // Scrolling horizontal scanlines, keyed off world height so they stay
    // stable in space as the object/camera move.
    float scan = sin((WorldPos.y * SCANLINE_DENSITY) - time * SCANLINE_SPEED * SCANLINE_DENSITY * 0.02);
    scan = 0.5 + 0.5 * scan;             // 0..1
    float scanline = mix(0.6, 1.0, scan); // don't fully black out between lines

    // A couple of brighter horizontal "sweep" bars travelling up the model.
    float sweep = fract(WorldPos.y * 0.15 - time * 0.25);
    float sweepBar = smoothstep(0.0, 0.03, sweep) * (1.0 - smoothstep(0.05, 0.12, sweep));

    // Subtle global flicker (like an unstable projection).
    float flicker = 0.9 + 0.1 * sin(time * FLICKER_SPEED) * sin(time * FLICKER_SPEED * 0.37);

    vec3 tint = baseColor.rgb;
    vec3 result = tint * scanline * flicker;
    result += tint * fresnel * 1.5;      // bright glowing edge
    result += vec3(1.0) * sweepBar * 0.6; // white sweep highlight

    // More opaque at the glowing edges, more see-through in the flat center.
    float alpha = clamp(BASE_ALPHA + fresnel * 0.6 + sweepBar * 0.3, 0.0, 1.0) * baseColor.a;
    alpha *= scanline; // scanline gaps are slightly more transparent too

    FragColor = vec4(result, alpha);
}
