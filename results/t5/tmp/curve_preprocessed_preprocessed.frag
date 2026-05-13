uniform vec3      iResolution;           // viewport resolution (in pixels)
uniform float     iTime;                 // shader playback time (in seconds)
uniform float     iTimeDelta;            // render time (in seconds)
uniform float     iFrameRate;            // shader frame rate
uniform int       iFrame;                // shader playback frame
uniform float     iChannelTime[4];       // channel playback time (in seconds)
uniform vec3      iChannelResolution[4]; // channel resolution (in pixels)
uniform vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
uniform sampler2D iChannel0;          // input channel. XX = 2D/Cube
uniform sampler2D iChannel1;          // input channel. XX = 2D/Cube
uniform sampler2D iChannel2;          // input channel. XX = 2D/Cube
uniform sampler2D iChannel3;          // input channel. XX = 2D/Cube
uniform vec4      iDate;                 // (year, month, day, time in seconds)
uniform float     iSampleRate;           // sound sample rate (i.e., 44100)

// --- Heightmap Function ---
float map(vec2 p) {
    vec2 offset = vec2(sin(p.y + cos(p.x)), cos(p.x + sin(p.y)));
    return (sin(p.x + offset.x) + cos(p.y + offset.y)) * 0.5;
}

// --- Main Image ---
void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Normalize pixel coordinates (from -1 to 1)
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;

    // 1. Camera Setup
    // Position the camera slightly above the ground and tilted down
    vec3 ro = vec3(0.0, 3.0, iTime); // iTime moves the camera forward automatically
    vec3 rd = normalize(vec3(uv.x, uv.y - 0.5, 1.0)); // Ray direction

    // 2. Raymarching
    float t = 0.0;
    vec3 p = vec3(0.0);
    for(int i = 0; i < 100; i++) {
        p = ro + rd * t;
        float h = p.y - map(p.xz);
        
        if(h < 0.001 || t > 20.0) break;
        t += h * 0.5;
    }

    // 3. Rendering the Lines
    vec3 col = vec3(1.0); // Background color (White)

    if(t < 20.0) {
        // We hit the terrain!
        float height = p.y;
        
        // Multiply by line density. Higher number = more topographical lines
        float lines = fract(height * 10.0); 
        
        // Create crisp lines using smoothstep. 
        // We use fwidth to keep the line thickness consistent regardless of distance.
        float thickness = (abs(height * 10.0) * (1.0 / iResolution.y) + (1.0 / iResolution.y)) * 1.5;
        float lineMask = smoothstep(thickness, 0.0, lines);
        
        // Mix the terrain color (white) with the line color (black)
        col = mix(vec3(1.0), vec3(0.0), lineMask);
        
        // Optional: Add distance fog so lines fade out elegantly
        col = mix(col, vec3(1.0), 1.0 - exp(-0.05 * t));
    }

    // Output to screen
    fragColor = vec4(col, 1.0);
}
