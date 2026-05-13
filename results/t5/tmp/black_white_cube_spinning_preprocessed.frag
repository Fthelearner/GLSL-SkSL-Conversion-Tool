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
// almost time...  the squbes want to have a word with you...

#define T iTime
#define R(a) mat2(cos(a + vec4(0,33,11,0)))
#define N normalize

// Commutative smooth minimum function. Provided by Tomkh, and taken 
// from Alex Evans's (aka Statix) talk: 
// http://media.lolrus.mediamolecule.com/AlexEvans_SIGGRAPH-2015.pdf
// Credited to Dave Smith @media molecule.
float smin(float a, float b, float k){

   float f = max(0., 1. - abs(b - a)/k);
   return min(a, b) - k*.25*f*f;
}

float box(vec3 p, float i) {
    p = abs(fract(p/i)*i - i/2.) - i*.125;
    return min(p.x, min(p.y, p.z));
}

float boxen(vec3 p) {
    float d = -9e9, i = 2e1;
    p.xy *= R(T/1e1);
    for(; i > .65; i *= .5)
        d = max(d, box(p, i));
    return d;
}

float blobs(vec3 p) {

    p.xy += 1.5;

    vec3 q = p;
    float s1 = 0.0, s2 = 0.0;

    // s1
    p.z -=  7.;
    p.xz *= R(T/1e1);
    p += .15*(cos(.3*T+dot(cos(.4*T+p+cos(p)), p) *  p/2. ));
    s1 = pow(dot(p=p*p*p*p,p),.125) - 1.5;

    // s2
    q.z -= 7.;
    q.xy -= 2.5;
    q.yz *= R(T/1e1);
    s2 = pow(dot(q=q*q*q*q,q),.125) - 1.5;


    // smin(s1, s2, x)
    return smin(s1, s2, .8);
}

bool hit = false;
float map(vec3 p) {
    float s = blobs(p),
          b = boxen(p);
    hit = s < b;
    return min(s, b);
}

// @iq
float AO(in vec3 pos, in vec3 nor) {
        float sca = 2.0, occ = 0.0;
    for( int i=0; i<5; i++ ){
    
        float hr = 0.01 + float(i)*0.5/4.0;        
        float dd = map(nor * hr + pos);
        occ += (hr - dd)*sca;
        sca *= 0.7;
    }
    return clamp( 1.0 - occ, 0.0, 1.0 );    
}

void mainImage(out vec4 o, in vec2 u) {
    float s = 0.0, i = 0.0;

    vec3 r = iResolution;
    u = (u+u - r.xy) / r.y;
    
    if (abs(u.y) > .8) { o *=i; return; }


    vec3 p = vec3(0.0, 0.0, 0.0), D = N(vec3(u, 1));

    for(o *= i; i++ < 70.; )
        p += D * s,
        o += s = map(p);
    
    // normal
    // tetrahedron technique: https://iquilezles.org/articles/normalsSDF/
    const float h = 0.005;
    const vec2 k = vec2(1,-1);
    r = N(k.xyy*map( p + k.xyy*h ) + 
          k.yyx*map( p + k.yyx*h ) + 
          k.yxy*map( p + k.yxy*h ) + 
          k.xxx*map( p + k.xxx*h ) );

    // reflection march
    if(hit) {
        vec3 ref = vec3(0.0, 0.0, 0.0);
        for(p += r*.05, D = reflect(D, r), s=i=0.; i++<30.; )
            p += D*s,
            s = map(p)*.8,
            ref += s;

        o.rgb += ref/2.;
    }

    o *= AO(p, r);
    o = tanh(o/4e1);
}
