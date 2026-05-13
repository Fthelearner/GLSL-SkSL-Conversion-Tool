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

/*
A fork of https://www.shadertoy.com/view/tcByRt with lots of diatribes,
and a join with https://www.shadertoy.com/view/tXl3Wn or 4XGBzh (doesn't matter which).
A bit of a mess of inexact polyhedra with domain repetition the sometimes fails side-lighting. Oh, well.
I raised the orb a little to reduce visiblity of some errors, some of the time.
And around 800 sec the polys get spotty.
Maybe I'll figure this out when I have more time.
I won't be surprised if this earns a performance warning...
but I get 48fps Win Chrome and 20 on iPhone, normal window.
*/

// https://www.shadertoy.com/view/tcByRt
/*
Top half here is all diatribes. I am pretty sure he doesn't mind.
I was lazy and did not hunt for more from others. Sorry.
It is surprising how many I could add; It seems I could keep going.

Some little post posting changes:
Added the emergency exit signs on the ceiling I had intended.
(Now all 4 planes suggest movement that does not exist.)
Added some Phong reflection to delineate the frames from their shadows more.
*/

vec3 resol; // fake resolution
#define iResolution resol

// https://www.shadertoy.com/view/tcSyDm
void mainImage0(out vec4 o, vec2 u) {
    float i,a,d,s,t=iTime;
    vec3  p = iResolution;
    u = (u-p.xy/2.)/p.y;
    for(o*=i; i++<1e2;
        d += s = .01+abs(s) * .6,
        o += vec4(14,2.7-cos(.5*t)*.6,.8,0)/s)
        for (p = vec3(u * d, d - 9.),
            p.z *= .3,
            p.xy *= mat2(cos(.01*t+p.z*d*.005+vec4(0,33,11,0))),
            s = max(6. - length(p.xy), length(p) - 16.),
            a = 1.; a < 8.; a += a)
            p += cos(.2*t+a+p.yzx)*.3,
            s -= abs(dot(sin(t+p * a * 6.), .03+p-p)) / a;
    o = tanh(o / 1e4 / max(dot(u += u.yx * 2.,u), .1));
}

// https://www.shadertoy.com/view/3cjcWD#
void mainImage1(out vec4 o, vec2 u) {
    float d,a,e,i,s,t = iTime*.5;
    vec3  ep, p = iResolution;    
    u = (u+u-p.xy)/p.y;    
    u += vec2(cos(t*.4)*.3, cos(t*.8)*.1);   
    for(o*=i; i++<1e2;
        d += s = min(.02+.6*abs(s),e=max(.8*e, .01)),
        o += 1./(s+e*2.))
        for (p = vec3(u*d,d+t), // p = ro + rd *d, p.z + t;
            ep = p - vec3(
                sin(sin(t)+t*.4) * 8.,
                sin(sin(t)+t*.2) *2.,
                16.+t+cos(t)*8.),
            e = length(ep) - .1,
            s = mix(e*.02,4.+p.y, smoothstep(0., 12., length(ep))),
            a = .4; a < 8.; a *= 1.4)
            s -= abs(dot(cos(t+.2*p.z+p * a ), .11+p-p)) / a;    
    o = tanh(vec4(1,2,6,0)*o/1e1/length(u-.65));
}

// https://www.shadertoy.com/view/wcsczf
void mainImage2(out vec4 o, vec2 u) {
    float i,d,s,n,t = iTime, a;
    vec3 p = iResolution;   
    u = (u-p.xy/2.)/p.y;  
    for(o*=i; i++<128.;d += s = .003+abs(s) * .8, o += vec4(32,4,16, 0)/(s+a))
        for (p = vec3(u * d, d+t*4.),
             p.xy *= mat2(cos(.1*t+p.z*.1+vec4(0,33,11,0))),
             s = 1.+sin(1.+p.y+p.x),
             n = 8.; n < 32.; n += n )
                 a = abs(dot(cos(p*n*s*s*d*2.), sin(p))) / n,
                 s -= a;               
    o = tanh(mix(o, o.yzxw, i=length(u))/2e5/i);
}

// https://www.shadertoy.com/view/3cXyDH
#define P(z) vec3(tanh(cos((z) * .31) * .6) * 6., \
                  tanh(cos((z) * .33) * .5) * 6., (z))
#define T ((sin(iTime*.3)+iTime*.3)) * 2.5
void mainImage3( out vec4 o, in vec2 u )
{
    float s=.002,d=0.,i,l,w,t=iTime*4.,f=(sin(iTime*1.5)*35.+60.);
    vec3  r = iResolution,
        p = P(T),ro = p,
        Z = normalize( P(T+3.) - (p - vec3(
                                  P(p.z).x+tanh(cos(t * .3)*2.) * 1.8,
                                  P(p.z).y+tanh(cos(t * .5)*2.) * 1.8,
                                  1.3+T+tanh(cos(t*.125)*9.)*9.)) - p),
        X = normalize(vec3(Z.z,0,-Z)),
        D = vec3(mat2(cos(sin(p.z*.3)*.3+vec4(0,33,11,0)))*(u-r.xy/2.)/r.y, 1) 
                * mat3(-X, cross(X, Z), Z);
    for(o*=0.; i++ < f && s > .001;d += s = length(p)/w) {
        p = ro + D * d;
        p.xy -= P(p.z).xy;
        p.xy *= .5;
        p.y -= 1.5;
        w = .8;
        for (int j; j++ < 8; p *= l, w *= l )
            p  = abs(sin(p)) - 1.,
            l = 1.6/dot(p,p);
    }
    o += tanh(.8/d);
}
#undef T
#undef P

// https://www.shadertoy.com/view/W3KSWw
#define P(z) vec3(cos((z) * .09)* 8., cos((z) * .05)* 12., (z))
#define N(f, i, s) abs(dot(sin(f*p*s), i +p-p )) / s
void mainImage4(out vec4 o, in vec2 u) {
   float l,i,d,s=.002,T = iTime*6.;
    vec3  q = iResolution,
          p = P(T),ro=p,
          Z = normalize( P(T+4.) - p),
          X = normalize(vec3(Z.z,0,-Z)),
          D = vec3((u-q.xy/2.)/q.y, 1) 
              * mat3(-X, cross(X, Z), Z);
    for(o *= i; i++<1e2;o+=(1.+cos(.1*p.z+vec4(3,1,0,0)))/s)
        p = ro + D * d,
        q = P(p.z),
        l = abs(length(p - vec3(
                       q.x+sin( sin(p.z*.3) ), 
                       q.y+sin( sin(p.z*.1) ),
                       12.  +T +cos(T*.15) *7. )  ) - .5),
        s = cos(p.z*.1)*3.+6. - 
            min(length(p.xy - q.y- 4.),
            min(length(p.xy - q.xy),
                length(p.x - q.y - 12.))),
        s -= N(.1*p.z+2., .2, 1.),
        s += N(.6*p.z+2., .2, 2.),
        s += tanh(1.+dot(tanh(p*.3), cos(p+sin(p.zxy)))),
        s *= .4, // clean up surface texture
        d += s = .01+.65*abs(min(s,l));
    o = tanh(o/2e3);
}
#undef P
#undef N


// https://www.shadertoy.com/view/t3yXRm
#define O(f,Z,c) abs( length(                 /* orb */   \
          p - vec3( q.x+sin( sin(p.z*f) ) * 3. ,      \
                    q.y+sin( sin(p.z*f*.5) ) * 2.,       \
                    Z  +T +6.+cos(T*.05) *6. )  ) - c ) 
#define P(z) vec3(cos((z) * .09)* 8., cos((z) * .05)* 12., (z))
#define N(f, i, s) abs(dot(sin(f*p*s), i +p-p )) / s
void mainImage5(out vec4 o, in vec2 u) {
    float l,i,d,s=.002,T = iTime*9.;
    vec3  q = iResolution,
          p = P(T),ro=p,
          Z = normalize( P(T+4.) - p),
          X = normalize(vec3(Z.z,0,-Z)),
          D = vec3((u-q.xy/2.)/q.y, 1) 
              * mat3(-X, cross(X, Z), Z);
    for(o *= i; i++<1e2;o+=1./s/l)
        p = ro + D * d,
        q = P(p.z),
        l = min( O(.1, 8., .4),
            min( O(.2, 7., .2),
                 O(.3, 9., .3) )),
        s = cos(p.z*.2)*2.+3.5 - 
            min(length(p.xy - q.x - 16.),
            min(length(p.xy - q.xy),
                length(p.x - q.y + 12.))),
        s += N(2., .4, 1.),
        s += N(2., .5, 16.),
        d += s = .003+.3*abs(min(s,l));
    o = tanh(vec4(4,1,6,0)*o/2e3);
}
#undef O
#undef P
#undef N


// https://www.shadertoy.com/view/t3tXDs
#define T iTime
#define O(f,Z,c) abs( length(                 /* orb */   \
          p - vec3( sin( sin(p.z*f*.5 ) +T*.7 ) * 3. ,        \
                    sin( sin(p.z*f*1.3) +T*.5 ) * 2.,  \
                    Z +8. +T*3. +cos(T*.3) *8. )  ) - c ) 
void mainImage6(out vec4 o, vec2 u) {
    float l,  s,  d, i, n;
    vec3 p = iResolution;
    u = (u-p.xy/2.)/p.y;
    for(o*=i; i++<60.;d += s = .001+abs(min(s,l))*.5, o += 1./s/l)
        for (p = vec3(u * d, d+T*3.),
             p = abs(p),
             s = tanh(4.-abs(p.x)),
             l = .01 + .8 *  min( O(.5, 6., .4),
                             min( O(.4, 3., .2),
                                  O(.3, 4., .3) )),
             n = 1.; n < 6.; n *= 1.3 )
                 s += abs(dot(cos(.5*T+p.z+p*n), vec3(.3))) / n;                 
   o = tanh(2.*abs(vec4(.1,4./d, d/3e1,0)) * o/1e2/max(d,15.) / max(length(u), .001));
   o = mix(o.zyxw, o.yxzw, smoothstep(0., 1., length(u)*2.));
}
#undef O
#undef T

// https://www.shadertoy.com/view/33KSWz
void mainImage7(out vec4 o, vec2 u) {
    float i, d ,s,n,t = iTime;
    vec3 p = iResolution;
    u = (u-p.xy/2.)/p.y;
    u += vec2(cos(t*.6)*.2, sin(t*.4)*.25);
    for(o*=i; i++<1e2;d += s = .001+abs(s)*.7, o += 1./s)
        for (p = vec3(u * d, d+t*4.),
             p.xy *= mat2(cos(.2*t+p.z*.1+vec4(0,33,11,0))),
             p.xy /= sin(p.x + cos(p.y)),
             s = tanh(1.+p.y),
             n = 2.; n < 16.; n *= 1.42 )
                 s += abs(dot(step(1./d, cos(t+p.z+p*n)), vec3(.4))) / n;
    o = tanh(mix(o=vec4(8,1,3,4)*o / 2e3 /d, o.xzyw, smoothstep(0.,1.,length(u))));
}


// https://www.shadertoy.com/view/t3tSWs
#define T iTime
#define O(f,Z,c) abs( length(                 /* orb */   \
          p - vec3( sin( sin(p.z*f*.5 ) +T*.7 ) * 3. ,        \
                    sin( sin(p.z*f*1.3) +T*.5 ) * 3. + 1.,  \
                    Z +8. +T +cos(T*.3) *16. )  ) - c ) 
void mainImage8(out vec4 o, vec2 u) {
    float l, s, d, i, n; 
        vec3 p = iResolution;
    u = (u-p.xy/2.)/p.y;
    u += vec2(cos(T*.3)*.2, sin(T*.2)*.15);
    for(o*=i; i++<70.;d += s = .001+abs(min(s,l))*.7, o += 1./s/l)
        for (p = vec3(u * d, d+T),
             p.xy *= mat2(cos(.15*T-p.z*.15+vec4(0,33,11,0))),
             s = tanh(1.-abs(p.x)),
             l = .005 + .8 * min( O(.3, 16., .4),
                             min( O(.1, 13., .2),
                                  O(.2, 14., .3) )),
             n = 2.; n < 16.; n *= 1.42 )
                 s += abs(dot(round(cos(T+p.z+p*n)), vec3(.5))) / n;
    o = tanh(2.*abs(vec4(.1,4./d, d/3e1,0)) * o/1e2/max(d,15.) / max(length(u), .001));
}
#undef O
#undef T


// https://www.shadertoy.com/view/WX3SD7
#define O(f,Z,c) abs( c - length(                 /* orb */   \
          p - vec3( sin( sin(p.z*f*.5 ) +T*.7 ) * 4. ,        \
                    sin( sin(p.z*f*1.3) +T*.5 ) * 1.23 + 1.,  \
                    Z +5. +T +cos(T*.5) *6. )  ) )
void mainImage9(out vec4 o, vec2 u) {
    float d,i,e,s,w,l, T = iTime;
    vec3  p = iResolution;
    u = ( u - p.xy/2. ) / p.y
       + vec2(sin(T*.2)*.3,
              sin(T*.5)*.1);
    for(o*=i; i++ < 80.; o += 1. / (s + e*3.) ) {
        p = vec3( u*d, d+T ),
        e = .01 + .8* min( O(.3, 6., .4),
                      min( O(.1, 3., .2),
                           O(.2, 4., .3) )),
        s = .001+abs(1.2+p.y)*.8;
        p.y *= .5, p.xy -= 1.5, w = 1.; // --- fractal(p)
        for (int i; i++ < 8; w *= l )
            p *= l = 3./dot( p = sin(p) , p);
        d += s = min( min(e,s),
                      length(p)/w ); // fractal(p)
    } 
    o = tanh(vec4(4,2,1,0)*o*o/4e2);
}
#undef O

// https://www.shadertoy.com/view/w3cSzn
void mainImage10(out vec4 o, vec2 u) {
    float i,d,s,t=iTime;
    vec3  p,r = iResolution;
    for(o*=i;
        i++<64.;
        o += (1.+cos(.3*p.z+vec4(6,2,3,1)))/max(s,.01))
        d += s = .5 * abs(s) + .005,
        p = vec3((u+u-r.xy)/r.y * d, d + t)+1.,
        p.xy *= mat2(cos(.3*t+p.z*.2+vec4(0,33,11,0))),
        p = tanh(sin(t*.3)*4.)*2.3+11. - abs(8. - abs( 2. - abs(p))),
        s = cos(p.x+p.y)+abs(dot(sin(p * 16.), .1+p-p));
    o = tanh(o / 3e3);
}

// ***************************************************************************

#undef iResolution


// ***************************************************************************
// https://www.shadertoy.com/view/tXl3Wn or 4XGBzh (doesn't matter which).

#define PI (3.14159265)
#define TAU (2.*PI)

#define rot2d(ang) mat2(cos(ang),-sin(ang),sin(ang),cos(ang))

// ********

// Relative sizes of Platonic Polyhedra 
// Compact inexact SDFs

#define max4(A,B,C,D) max(max(A,B),max(C,D))
#define max3(A,B,C) max(max(A,B),C)

// Circumscribed sphere radius ratio to Inscribed sphere radius (thanks spalmer)
#define CircSphTetrahedron  (1./3.)
#define CircSphCube         sqrt(1./3.)
#define CircSphOctahedron   sqrt(1./3.)
#define CircSphDodecahedron sqrt(1./PHI)
#define CircSphIcosahedron  sqrt(1./PHI)


float sdTetrahedron(vec3 p,float r) {
    return max4(-p.x+p.y+p.z, p.x-p.y+p.z, p.x+p.y-p.z, -p.x-p.y-p.z)/sqrt(3.) - r*CircSphTetrahedron;
}

float sdCube(vec3 p,float r) {
    p = abs(p);
    return max3(p.x,p.y,p.z) - r*CircSphCube;
}

float sdOctahedron(vec3 p,float r) {
    p = abs(p);
    return (p.x+p.y+p.z)/sqrt(3.) - r*CircSphOctahedron;
}

#define PHI 1.6180339887

float sdDodecahedron(vec3 p,float r) {
    p = abs(p);
    return max3( p.y+p.z*PHI, p.z+p.x*PHI, p.x+p.y*PHI )/sqrt(2.+PHI) - r*CircSphDodecahedron;
}

float sdIcosahedron(vec3 p,float r) {
    p = abs(p);
    return max4( p.x+p.y+p.z, p.y/PHI+p.z*PHI, p.z/PHI+p.x*PHI, p.x/PHI+p.y*PHI )/sqrt(3.) - r*CircSphIcosahedron;
}    

float sdSphere(vec3 p,float r) {
    return length(p) - r;
}


float sdTorus( vec3 p, vec2 t ) // https://www.shadertoy.com/view/Xds3zN IQ
{
    return length( vec2(length(p.xz)-t.x,p.y) )-t.y;
}

// ********

#define ROT2(ANG) mat2(cos(ANG),sin(ANG),-sin(ANG),cos(ANG))

vec3 Spin(float tim,vec3 p) {
   tim += iTime;
   p.xz *= ROT2(tim*.5);
   p.yz *= ROT2(tim*.7);
   p.yx *= ROT2(tim*.4);
   return p;
}

vec2 Dist(vec3 pt) { // distance, color <-- was never color but hue, so another change ...
    vec2 hit = vec2(100000,0);
    float tmp, clr = 0.;

#define T(SDF,CLR) if ( (tmp = SDF) < hit.x ) hit = vec2(tmp,CLR);

    { vec3 p = pt + vec3( 0, 0, iTime*40. ), q = p;
    p.z = mod( p.z, 40. );
    q.z -= p.z;
    p.y += 25. - 7.5;
    float t = iTime * 2.;
    #define POP(SDF,X,Z,O) T( SDF(  Spin(t+O,p-vec3(X/4.+3.*sin(q.z),20.*abs(sin(t+O+q.z))-6.,Z)),2.)  , O+floor(iTime/30./40.) )
    POP( sdIcosahedron,  -15., 20, 7. )
    POP( sdTetrahedron,  -10., 30, 1. )
    POP( sdOctahedron,     7., 40, 2. )
    POP( sdDodecahedron,  -7., 40, 3. )
    POP( sdCube,          15., 25, 4. )
    POP( sdSphere,        20., 25, 6. )
    //T( sdIcosahedron(pt-vec3(15,250.*abs(sin(iTime))-120.,400),4.), 0. )
    //T( sdDodecahedron(pt-vec3(-5,250.*abs(sin(iTime+4.))-140.,350),4.), 3. )
    //T( sdSphere(pt-vec3(-15,250.*abs(sin(iTime+4.))-100.,350),4.), 6. )
    T( sdTorus( (p - vec3(-20,30.*abs(sin(iTime*1.6+q.z))-3.5,30)).yxz, vec2(3.0,1.) ) , 5. )
    }

    //T( pt.y+5, 85. ); 

    hit.y *= 123.;
    return hit;
}
#undef T
#undef POP

/*
#define ZERO (min(iFrame,0)) // trick to avoid loop unrolling

vec4 March(vec3 beg,vec3 dir) { // return intersection point and object of ray
    float dist = 0.;
    vec3 pos;
    for ( int stps = ZERO; stps < 300+1; ++stps ) {
        pos = beg + dir * dist;
        vec2 obj = Dist( pos );
        dist += obj.x;
        if ( dist > 800. ) return vec4( pos, 91. );
        if ( pos.z < -80. ) return vec4( pos, 91. );
        if ( obj.x < .01 || stps == 300 ) return vec4( pos, obj.y );
    }
    return vec4( pos, 90. );
}


vec3 Normal(vec3 pt) {
    float delta = pt.z/200.; // large delta gives rounded corners
    vec3 norm = Dist(pt).x - vec3(
        Dist(pt-vec3(delta, 0., 0.)).x, 
        Dist(pt-vec3( 0.,delta, 0.)).x, 
        Dist(pt-vec3( 0., 0.,delta)).x );
    return normalize( norm );
}
*/

// ********

/*
void QmainImage( out vec4 O, vec2 U )
{
    vec2 R = iResolution.xy;
    U.y += 100.;
    vec2 uv = (U+U-R)/R.y;
    vec3 dir = normalize( vec3( uv, 2.5 ) );
        vec3 cam = vec3(0);

    mat2 rot = rot2d(sin(iTime)*.02);
        dir.xz *= rot;
        dir.yz *= rot2d(cos(iTime)*.02);;
    
    vec4 hit = March( cam, dir );

    vec3 back=dir;
    back.xz *= rot2d(PI/2.);
    O = texture( iChannel0, back );

    vec3 Light = vec3( -20., 10, -30 );
    vec3 ldir = normalize( Light - hit.xyz );

    vec3 norm = Normal(hit.xyz);
    float difu = dot( norm, ldir );
    
    if ( hit.w == 85. ) {
        // Position shadow slightly behind object from camera view to slightly reduced visible errors with bounces before touching ground
        vec4 bl = March( hit.xyz+vec3(hit.x*-.05,.005,-7.), normalize(vec3(0,1,0)) );
        if (bl.w<80.)
            O *= clamp( bl.y/3., .6, 1. );
    }
    
    #define color4(X) ( .5 + .3 * sin( vec4(0,21,23,0) + (X) ) )
    
    if ( hit.w < 80. ) {
    
    O = color4(hit.w*.4);
    
    O *= max(.2,difu)*.8;
    
    O = mix(O,vec4(.5),max(0.,(hit.z-100.)/1000.));

    O = sqrt(O);
    }

}
*/

#define color3(X) ( .5 + .3 * sin( vec3(0,21,23) + (X) ) )

// ************************************

//#define PI 3.14159265
//#define TAU (2.*PI)

vec3 light = vec3(-10);

float sdSphere(vec3 p) {
    return length(p);
}

float sdBox(vec3 p,vec3 s) {
    s = abs(p) - s;
    return min(max(s.x,max(s.y,s.z)),0.) + length(max(s,0.));
}

vec2 sdExit(vec3 p) {
    //vl(1.,0.,1.,.2,-1.,.2,-1.,.0)
    //vl(1.,.2,1.,.5
    return vec2( sdBox(p,vec3(3,1.5,.2)), 656. );
}

float zoff; // only thing moving

#define T(SDF,CLR) if ( t=(SDF), t < r.x ) r = vec2(t,CLR);
vec2 map(vec3 p) { // distance, decimal RGB color
    vec2 r = vec2(9e9,0);
    float t;
  
    vec3 q = p;
    q.xy = abs(q.xy);
    q.z = mod(q.z+2.+zoff,24.);
    T(sdBox(q-vec3(24.9,0,12),
         vec3(.5,8.5,10.5)),666)

    q = p;
    q.z = mod( q.z+zoff, 80. ) - 40.;
    vec2 v = sdExit(q-vec3(0,23.5,0));
    if ( v.x < r.x ) r = v;

    //T(sdBox(p-vec3(24,0,0),vec3(5.)),844)   
    //T(sdSphere(p)-.5,844)   
    
    T(550.-abs(p.z),001)
    T(25.+p.y,552)
    T(25.-p.y,888) 
    T(25.-abs(p.x),595)
    T(sdSphere(p-light)-2.,999)
    
    //p.y += 25.;
    vec2 other = Dist(p);
    if ( other.x < r.x ) r = other;
    
    return r;
}

vec4 march(vec3 p,vec3 d) {  // x,y,z, RGB color
    for ( int k = 0; k < 100; ++k ) {
        vec2 t = map(p);
        if ( t.x < .001 ) return vec4( p, t.y );
        p += d*t.x;
    }
    return vec4(0);
}

vec3 normal(vec3 p) {
    float eps = .001;
    vec2 k = vec2(1,-1);
    return normalize(
        k.xyy*map(p+k.xyy*eps).x +
        k.yyx*map(p+k.yyx*eps).x +
        k.yxy*map(p+k.yxy*eps).x +
        k.xxx*map(p+k.xxx*eps).x );
}

#define R2(a) mat2(cos((a)+vec4(0,11,33,0)))
#define D(x,n) float(int(x)/n%10)
#define sinc(x) (sin(x)/(x))

void mainImage(out vec4 o,vec2 u)
{
    float t = iTime;
    zoff = iTime*10.;
    light = vec3( 17.*sin(t*2.), 6.+10.*cos(t/2.3), 50.*sin(t*1.5)+50. );
    vec3 r = iResolution;
    vec4 M = iMouse;
    float ang1 = M.x+M.y < 10. ? sin(t)*.2 : ((M.x-r.x/2.)*TAU/r.x*.70);
    float ang2 = M.x+M.y < 10. ? 0. : ((M.y-r.y/2.)*TAU/r.y*.50);
    vec3 ro = vec3(0.*sin(t),0.*cos(t),-10);
    vec3 rd = vec3( u+u-r.xy, r.y*2. );
    //rd.yz *= R2(sin(t/1.6)*.2);
    rd.yz *= R2(ang2);
    rd.xz *= R2(ang1);
    vec4 hit = march( ro, normalize(rd) );
    vec3 clr = vec3( D(hit.w,100), D(hit.w,10), D(hit.w,1) )/9.;
    if ( hit.w == 552. ) clr += fract((zoff+hit.z+hit.x)/5.)*.1;
    vec3 nrm = normal(hit.xyz);
    float ll = 1.;
    if ( hit.w != 999. ) {
  // if(false)
        if ( abs(hit.x) >= 24. &&
             abs(hit.y) <= 8. ) {
             float z = mod(hit.z+zoff,24.);
             if ( z <= 20. ) {
                 int n = int(hit.z+zoff)/24;
                 resol = vec3(20,16,1);
                 vec2 i = vec2(z,hit.y+8.);
                 if ( hit.x < 0. ) n += 5;
                 n %= 11;
                 switch(n) {
                     case 1: mainImage1(o,i); break;
                     case 2: mainImage2(o,i); break;
                     case 3: mainImage3(o,i); break;
                     case 4: mainImage4(o,i); break;
                     case 5: mainImage5(o,i); break;
                     case 6: mainImage6(o,i); break;
                     case 7: mainImage7(o,i); break;
                     case 8: mainImage8(o,i); break;
                     case 9: mainImage9(o,i); break;
                     case 10: mainImage10(o,i); break;
                     default: mainImage0(o,i); break;
                 }
                 o = mix( o, vec4(.0),  min( 1., hit.z/200. ) );
                 return;
             }
        }

        vec3 ln = normalize(light-hit.xyz);
        ll = max(0.,dot(nrm,ln));
        if (hit.w==666.) ll *= ll, ll *= ll;
        vec4 shad = march(hit.xyz+ln*.05,ln);
        if ( shad.w != 999. )
           ll *= .6;
        if (hit.w==656.)
           if ( (abs(hit.x)<1. && hit.y<23.)
                || ( hit.y>=23. && abs(hit.x)+hit.y<25.) ) 
               ll=1.,
               clr=vec3(1,0,0);
           
    }
    clr = mix( clr, vec3(0), min( 1., hit.z/200. ) );
    o = clr.rgbb * ll;
}

