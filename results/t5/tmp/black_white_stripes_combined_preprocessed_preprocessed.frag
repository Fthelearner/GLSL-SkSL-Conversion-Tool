#define MAX_STEPS 200
#define pi 3.14159

#define thc(a,b) tanh(a*cos(b))/tanh(a)
#define ths(a,b) tanh(a*sin(b))/tanh(a)
#define sabs(x) sqrt(x*x+1e-2)
//#define sabs(x, k) sqrt(x*x+k)

#define rot(a) mat2(cos(a), -sin(a), sin(a), cos(a))

float cc(float a, float b) {
    float f = thc(a, b);
    return sign(f) * pow(abs(f), 0.25);
}

float cs(float a, float b) {
    float f = ths(a, b);
    return sign(f) * pow(abs(f), 0.25);
}

vec3 pal(in float t, in vec3 d) {
    return 0.5 + 0.5 * cos(2. * pi * (0.5 * t + d));
}

vec3 pal(in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d) {
    return a + b * cos(2. * pi * (c * t + d));
}

float h21(vec2 a) {
    return fract(sin(dot(a.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

float mlength(vec2 uv) {
    return max(abs(uv.x), abs(uv.y));
}

float mlength(vec3 uv) {
    return max(max(abs(uv.x), abs(uv.y)), abs(uv.z));
}

float sfloor(float a, float b) {
    return floor(b) + 0.5 + 0.5 * tanh(a * (fract(b) - 0.5)) / tanh(0.5 * a);
}

// From iq, k = 0.12 is good
float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0., 1.);
    return mix(b, a, h) - k * h * (1. - h);
}

float smax(float a, float b, float k) {
    float h = clamp(0.5 - 0.5 * (b - a) / k, 0., 1.);
    return mix(b, a, h) + k * h * (1. - h); 
}uniform vec3      iResolution;           // viewport resolution (in pixels)
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
#define MAX_DIST 100.
#define SURF_DIST .001

// RayMarching from TheArtOfCode

// From BlackleMori
#define FK(k) floatBitsToInt(k*k/7.)^floatBitsToInt(k)
float hash(float a, float b) {
    int x = FK(a), y = FK(b);
    return float((x*x+y)*(y*y-x)-x)/2.14e9;
}

vec3 erot(vec3 p, vec3 ax, float ro) {
  return mix(dot(ax, p)*ax, p, cos(ro)) + cross(ax,p)*sin(ro);
}

vec3 face(vec3 p) {
     vec3 a = abs(p);
     return step(a.yzx, a.xyz)*step(a.zxy, a.xyz)*sign(p);
}

float sdBox(vec3 p, vec3 s) {
    p = abs(p)-s;
        return length(max(p, 0.))+min(max(p.x, max(p.y, p.z)), 0.);
}

float sdBox(in vec2 p, in vec2 b){
    vec2 d = abs(p)-b;
    return length(max(d,0.))+min(max(d.x,d.y),0.);
}

vec3 GetRayOrigin() {
    vec2 m = iMouse.xy/iResolution.xy;
    float r = 4.;
    float a = 0.15 * iTime;
    vec3 ro = vec3(r * cos(a), 1. + 1. * cos(0.4 * iTime), r * sin(a));
    //ro.yz *= rot(-m.y*3.14+1.);
    //ro.xz *= rot(-m.x*6.2831);
    return ro;
}

float GetDist(vec3 p) {
    p.xz *= rot(0.5 * iTime);
    p.x -= 1.;
    p.xz *= rot(-1.25 * iTime); 
    float r1 = 0.5;
    float r2 = 0.2;
    float td1 = length(p.xy) - r1;
    float td2 = length(vec2(td1, p.z)) - r2;
   
    float sd1 = td2;//sdBox(p - vec3(0,0.25 * cos(iTime),0), vec3(.6,.5,.2)) - 0.1;
    p.xz *= rot(1.25 * iTime);
    p.x += 1.;
    float sd2 = length(p + vec3(1,0.25 * sin(iTime),0)) - 0.5;
    float d = p.y + 1. - 0.001 * dot(p.xz,p.xz);
    d = min(d,sd1);
    d = min(d,sd2);
    return d;
}

float RayMarch(vec3 ro, vec3 rd, float z) {
        
    float dO=0.;
    float s = sign(z);
    for(int i=0; i<MAX_STEPS; i++) {
            vec3 p = ro + rd*dO;
        float dS = GetDist(p);
        if (s != sign(dS)) { z *= 0.5; s = sign(dS); }
        if(abs(dS)<SURF_DIST || dO>MAX_DIST) break;
        dO += dS*z; 
    }
    
    return min(dO, MAX_DIST);
}

vec3 GetNormal(vec3 p) {
        float d = GetDist(p);
    vec2 e = vec2(.001, 0);
    
    vec3 n = d - vec3(
        GetDist(p-e.xyy),
        GetDist(p-e.yxy),
        GetDist(p-e.yyx));
    
    return normalize(n);
}

vec3 GetRayDir(vec2 uv, vec3 p, vec3 l, float z) {
    vec3 f = normalize(l-p),
        r = normalize(cross(vec3(0,1,0), f)),
        u = cross(f,r),
        c = f*z,
        i = c + uv.x*r + uv.y*u,
        d = normalize(i);
    return d;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = (fragCoord-.5*iResolution.xy)/iResolution.y;
        
    vec3 ro = GetRayOrigin();
    
    vec3 rd = GetRayDir(uv, ro, vec3(0), 1.);
    vec3 col = vec3(0);
   
    float d = RayMarch(ro, rd, 1.);
    
    vec3 p = ro + rd * d;
    float IOR = 1.5;
    if(d<MAX_DIST) {        
        vec3 n = GetNormal(p);
        vec3 r = reflect(rd, n);
        if (p.y > -0.9)
            r = reflect(rd, n * 50. * exp(-3. * length(p.xz)));

        vec3 pIn = p - 4. * SURF_DIST * n;
        vec3 rdIn = refract(rd, n, 1./IOR);
        float dIn = RayMarch(pIn, rdIn, -1.);
        
        vec3 pExit = pIn + dIn * rdIn;
        vec3 nExit = -GetNormal(pExit); // *-1.; ?

        vec3 p2 = p + 4. * SURF_DIST * n;
        vec3 cr = normalize(p - pExit);
        float d2 = RayMarch(p2, cr, 1.);
        vec3 p3 = p2 + d2 * cr;
        vec3 n2 = GetNormal(p3);
        

        float dif = dot(n, normalize(vec3(.5*cos(iTime),1,.5*sin(iTime))))*.5+.5;
        float dif2 = dot(n2, normalize(vec3(1,2,3)))*.5+.5;
        //dif2 = pow(dif2, 5.);
        col = vec3(1);
        col *= dif;
        float k = 0.35;
       // col *= smoothstep(-k, k, dif2 - 0.8);
        col *= 0.5 + 0.5 * thc(4., 2. * iTime - 0.5 * length(p.xz) + 50. * abs(dif));//* abs(2. * dif-dif2));
        col *= exp(-0.5 * length(p));
        col *= 2. * dif2;
        float fres = pow(1. + dot(rd, n), 5.);
        float fres2 = pow(1. + dot(rd, nExit), 4.);
        float fog = 1.-exp(-length(p));

        float spec = pow(dif, 15.);
        float csh = 1. / cosh(0.2 * length(p.xz));
        col = clamp(col, 0., 1.);
        col = mix(col, vec3(abs(r)) * csh, fres);
        if (p.y < -0.9) {
            float tns = 1.;
            col += 0.6 * thc(4., iTime + 6. * log(length(p.xz))) * csh;
        }
    }
    
    col = pow(col, vec3(.4545));        // gamma correction
    
    fragColor = vec4(col,1.0);
}