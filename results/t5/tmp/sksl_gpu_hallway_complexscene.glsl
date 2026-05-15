### Compilation failed:

error: 22: unknown identifier 's'
    for (; i++ < 100.0; d += (s = 0.01 + abs(s) * 0.6), o += float4(14.0, 2.7 - cos(0.5 * t) * 0.6, 0.8, 0.0) / s) {
                              ^
error: 22: unknown identifier 's'
    for (; i++ < 100.0; d += (s = 0.01 + abs(s) * 0.6), o += float4(14.0, 2.7 - cos(0.5 * t) * 0.6, 0.8, 0.0) / s) {
                                             ^
error: 22: unknown identifier 's'
...++ < 100.0; d += (s = 0.01 + abs(s) * 0.6), o += float4(14.0, 2.7 - cos(0.5 * t) * 0.6, 0.8, 0.0) / s) {
                                                                                                       ^
error: 23: unknown identifier 's'
...9.0), p.b *= 0.3, p.xy *= float2x2(cos(0.01 * t + p.b * d * 0.005 + float4(0.0, 33.0, 11.0, 0.0))), s = max(6.0 - length(p.xy), length(p) - 16.0), a = 1.0;
                                                                                                       ^
error: 23: unknown identifier 'a'
... + p.b * d * 0.005 + float4(0.0, 33.0, 11.0, 0.0))), s = max(6.0 - length(p.xy), length(p) - 16.0), a = 1.0;
                                                                                                       ^
error: 25: unknown identifier 's'
            p += cos(0.2 * t + a + p.yzx) * 0.3, s -= abs(dot(sin(t + p * a * 6.0), 0.03 + p - p)) / a;
                                                 ^
error: 39: unknown identifier 's'
    for (; i++ < 100.0; d += (s = min(0.02 + 0.6 * abs(s), e = max(0.8 * e, 0.01))), o += 1.0 / (s + e * 2.0)) {
                              ^
error: 39: unknown identifier 's'
    for (; i++ < 100.0; d += (s = min(0.02 + 0.6 * abs(s), e = max(0.8 * e, 0.01))), o += 1.0 / (s + e * 2.0)) {
                                                       ^
error: 39: unknown identifier 'e'
    for (; i++ < 100.0; d += (s = min(0.02 + 0.6 * abs(s), e = max(0.8 * e, 0.01))), o += 1.0 / (s + e * 2.0)) {
                                                           ^
error: 39: unknown identifier 'e'
    for (; i++ < 100.0; d += (s = min(0.02 + 0.6 * abs(s), e = max(0.8 * e, 0.01))), o += 1.0 / (s + e * 2.0)) {
                                                                         ^
error: 39: unknown identifier 's'
    for (; i++ < 100.0; d += (s = min(0.02 + 0.6 * abs(s), e = max(0.8 * e, 0.01))), o += 1.0 / (s + e * 2.0)) {
                                                                                                 ^
error: 39: unknown identifier 'e'
...   for (; i++ < 100.0; d += (s = min(0.02 + 0.6 * abs(s), e = max(0.8 * e, 0.01))), o += 1.0 / (s + e * 2.0)) {
                                                                                                       ^
error: 40: unknown identifier 'ep'
        p = float3(u * d, d + t), ep = p - float3(sin(sin(t) + t * 0.4) * 8.0, sin(sin(t) + t * 0.2) * 2.0, 16.0 + t + cos(t) * 8.0), e ...
                                  ^^
error: 40: unknown identifier 'e'
...ep = p - float3(sin(sin(t) + t * 0.4) * 8.0, sin(sin(t) + t * 0.2) * 2.0, 16.0 + t + cos(t) * 8.0), e = length(ep) - 0.1, s = mix(e * 0.02, 4.0 + p.g, smoothstep(0.0, 12.0, length(ep))), a = 0.4;
                                                                                                       ^
error: 40: unknown identifier 'ep'
...oat3(sin(sin(t) + t * 0.4) * 8.0, sin(sin(t) + t * 0.2) * 2.0, 16.0 + t + cos(t) * 8.0), e = length(ep) - 0.1, s = mix(e * 0.02, 4.0 + p.g, smoothstep(0.0, 12.0, length(ep))), a = 0.4;
                                                                                                       ^^
error: 40: unknown identifier 's'
...n(t) + t * 0.4) * 8.0, sin(sin(t) + t * 0.2) * 2.0, 16.0 + t + cos(t) * 8.0), e = length(ep) - 0.1, s = mix(e * 0.02, 4.0 + p.g, smoothstep(0.0, 12.0, length(ep))), a = 0.4;
                                                                                                       ^
error: 40: unknown identifier 'e'
... * 0.4) * 8.0, sin(sin(t) + t * 0.2) * 2.0, 16.0 + t + cos(t) * 8.0), e = length(ep) - 0.1, s = mix(e * 0.02, 4.0 + p.g, smoothstep(0.0, 12.0, length(ep))), a = 0.4;
                                                                                                       ^
error: 40: unknown identifier 'ep'
... t + cos(t) * 8.0), e = length(ep) - 0.1, s = mix(e * 0.02, 4.0 + p.g, smoothstep(0.0, 12.0, length(ep))), a = 0.4;
                                                                                                       ^^
error: 40: unknown identifier 'a'
...s(t) * 8.0), e = length(ep) - 0.1, s = mix(e * 0.02, 4.0 + p.g, smoothstep(0.0, 12.0, length(ep))), a = 0.4;
                                                                                                       ^
error: 42: unknown identifier 's'
            s -= abs(dot(cos(t + 0.2 * p.b + p * a), 0.11 + p - p)) / a;
            ^
error: 53: unknown identifier 'i'
    o *= i;
         ^
error: 54: unknown identifier 's'
    for (float i = 0.0; i++ < 128.0; d += (s = 0.003 + abs(s) * 0.8), o += float4(32.0, 4.0, 16.0, 0.0) / (s + a)) {
                                           ^
error: 54: unknown identifier 's'
    for (float i = 0.0; i++ < 128.0; d += (s = 0.003 + abs(s) * 0.8), o += float4(32.0, 4.0, 16.0, 0.0) / (s + a)) {
                                                           ^
error: 54: unknown identifier 's'
... (float i = 0.0; i++ < 128.0; d += (s = 0.003 + abs(s) * 0.8), o += float4(32.0, 4.0, 16.0, 0.0) / (s + a)) {
                                                                                                       ^
error: 54: unknown identifier 'a'
...oat i = 0.0; i++ < 128.0; d += (s = 0.003 + abs(s) * 0.8), o += float4(32.0, 4.0, 16.0, 0.0) / (s + a)) {
                                                                                                       ^
error: 55: unknown identifier 's'
...at3(u * d, d + t * 4.0), p.xy *= float2x2(cos(0.1 * t + p.b * 0.1 + float4(0.0, 33.0, 11.0, 0.0))), s = 1.0 + sin(1.0 + p.g + p.r), n = 8.0;
                                                                                                       ^
error: 55: unknown identifier 'n'
... float2x2(cos(0.1 * t + p.b * 0.1 + float4(0.0, 33.0, 11.0, 0.0))), s = 1.0 + sin(1.0 + p.g + p.r), n = 8.0;
                                                                                                       ^
error: 57: unknown identifier 'a'
            a = abs(dot(cos(p * n * s * s * d * 2.0), sin(p))) / n, s -= a;
            ^
error: 57: unknown identifier 's'
            a = abs(dot(cos(p * n * s * s * d * 2.0), sin(p))) / n, s -= a;
                                    ^
error: 57: unknown identifier 's'
            a = abs(dot(cos(p * n * s * s * d * 2.0), sin(p))) / n, s -= a;
                                        ^
error: 57: unknown identifier 's'
            a = abs(dot(cos(p * n * s * s * d * 2.0), sin(p))) / n, s -= a;
                                                                    ^
error: 57: unknown identifier 'a'
            a = abs(dot(cos(p * n * s * s * d * 2.0), sin(p))) / n, s -= a;
                                                                         ^
error: 60: unknown identifier 'i'
    o = tanh(mix(o, o.yzxw, i = length(u)) / 200000.0 / i);
                            ^
error: 60: unknown identifier 'i'
    o = tanh(mix(o, o.yzxw, i = length(u)) / 200000.0 / i);
                                                        ^
error: 74: invalid arguments to 'float3' constructor (expected 3 scalars, but found 5)
    float3 X = normalize(float3(Z.b, 0.0, -Z));
                         ^^^^^^^^^^^^^^^^^^^^
error: 75: unknown identifier 'X'
...os(sin(p.b * 0.3) * 0.3 + float4(0.0, 33.0, 11.0, 0.0))) * (u - r.xy / 2.0) / r.g, 1.0) * float3x3(-X, cross(X, Z), Z);
                                                                                                       ^
error: 75: unknown identifier 'X'
...b * 0.3) * 0.3 + float4(0.0, 33.0, 11.0, 0.0))) * (u - r.xy / 2.0) / r.g, 1.0) * float3x3(-X, cross(X, Z), Z);
                                                                                                       ^
error: 77: unknown identifier 'w'
    for (; i++ < f && s > 0.001; d += (s = length(p) / w)) {
                                                       ^
error: 78: unknown identifier 'D'
        p = ro + D * d;
                 ^
error: 83: unknown identifier 'l'
        for (; j++ < 8; p *= l, w *= l) {
                             ^
error: 83: unknown identifier 'l'
        for (; j++ < 8; p *= l, w *= l) {
                                     ^
error: 84: unknown identifier 'l'
            p = abs(sin(p)) - 1.0, l = 1.6 / dot(p, p);
                                   ^
error: 99: invalid arguments to 'float3' constructor (expected 3 scalars, but found 5)
    float3 X = normalize(float3(Z.b, 0.0, -Z));
                         ^^^^^^^^^^^^^^^^^^^^
error: 100: unknown identifier 'X'
    float3 D = float3((u - q.xy / 2.0) / q.g, 1.0) * float3x3(-X, cross(X, Z), Z);
                                                               ^
error: 100: unknown identifier 'X'
    float3 D = float3((u - q.xy / 2.0) / q.g, 1.0) * float3x3(-X, cross(X, Z), Z);
                                                                        ^
error: 103: unknown identifier 'D'
        p = ro + D * d, q = float3(cos(p.b * 0.09) * 8.0, cos(p.b * 0.05) * 12.0, p.b), l = abs(length(p - float3(q.r ...
                 ^
error: 103: unknown identifier 'l'
        p = ro + D * d, q = float3(cos(p.b * 0.09) * 8.0, cos(p.b * 0.05) * 12.0, p.b), l = abs(length(p - float3(q.r + sin(sin(p.b * 0.3)), q.g + sin(sin(p.b * 0.1)), 12.0 + T + cos(T * 0....
                                                                                        ^
error: 103: unknown identifier 'l'
...= tanh(1.0 + dot(tanh(p * 0.3), cos(p + sin(p.zxy)))), s *= 0.4, d += (s = 0.01 + 0.65 * abs(min(s, l)));
                                                                                                       ^
error: 117: invalid arguments to 'float3' constructor (expected 3 scalars, but found 5)
    float3 X = normalize(float3(Z.b, 0.0, -Z));
                         ^^^^^^^^^^^^^^^^^^^^
error: 118: unknown identifier 'X'
    float3 D = float3((u - q.xy / 2.0) / q.g, 1.0) * float3x3(-X, cross(X, Z), Z);
                                                               ^
error: 118: unknown identifier 'X'
    float3 D = float3((u - q.xy / 2.0) / q.g, 1.0) * float3x3(-X, cross(X, Z), Z);
                                                                        ^
error: 120: unknown identifier 'l'
    for (; i++ < 100.0; o += 1.0 / s / l) {
                                       ^
error: 121: unknown identifier 'D'
        p = ro + D * d, q = float3(cos(p.b * 0.09) * 8.0, cos(p.b * 0.05) * 12.0, p.b), l = min(abs(length(p - float3(...
                 ^
error: 121: unknown identifier 'l'
        p = ro + D * d, q = float3(cos(p.b * 0.09) * 8.0, cos(p.b * 0.05) * 12.0, p.b), l = min(abs(length(p - float3(q.r + sin(sin(p.b * 0.1)) * 3.0, q.g + sin(sin(p.b * 0.1 * 0.5)) * 2.0,...
                                                                                        ^
error: 121: unknown identifier 'l'
...) / 1.0, s += abs(dot(sin(2.0 * p * 16.0), 0.5 + p - p)) / 16.0, d += (s = 0.003 + 0.3 * abs(min(s, l)));
                                                                                                       ^
error: 132: unknown identifier 's'
    for (; i++ < 60.0; d += (s = 0.001 + abs(min(s, l)) * 0.5), o += 1.0 / s / l) {
                             ^
error: 132: unknown identifier 's'
    for (; i++ < 60.0; d += (s = 0.001 + abs(min(s, l)) * 0.5), o += 1.0 / s / l) {
                                                 ^
error: 132: unknown identifier 'l'
    for (; i++ < 60.0; d += (s = 0.001 + abs(min(s, l)) * 0.5), o += 1.0 / s / l) {
                                                    ^
error: 132: unknown identifier 's'
    for (; i++ < 60.0; d += (s = 0.001 + abs(min(s, l)) * 0.5), o += 1.0 / s / l) {
                                                                           ^
error: 132: unknown identifier 'l'
    for (; i++ < 60.0; d += (s = 0.001 + abs(min(s, l)) * 0.5), o += 1.0 / s / l) {
                                                                               ^
error: 133: unknown identifier 's'
        p = float3(u * d, d + iTime * 3.0), p = abs(p), s = tanh(4.0 - abs(p.r)), l = 0.01 + 0.8 * min(abs(length(p - float3(sin(sin(p.b * 0.5 * 0.5) + iTime...
                                                        ^
error: 133: unknown identifier 'l'
        p = float3(u * d, d + iTime * 3.0), p = abs(p), s = tanh(4.0 - abs(p.r)), l = 0.01 + 0.8 * min(abs(length(p - float3(sin(sin(p.b * 0.5 * 0.5) + iTime * 0.7) * 3.0, sin(sin(p.b...
                                                                                  ^
error: 133: unknown identifier 'n'
...(sin(p.b * 0.3 * 1.3) + iTime * 0.5) * 2.0, 12.0 + iTime * 3.0 + cos(iTime * 0.3) * 8.0)) - 0.3))), n = 1.0;
                                                                                                       ^
error: 135: unknown identifier 's'
            s += abs(dot(cos(0.5 * iTime + p.b + p * n), float3(0.3, 0.3, 0.3))) / n;
            ^
error: 150: unknown identifier 's'
    for (; i++ < 100.0; d += (s = 0.001 + abs(s) * 0.7), o += 1.0 / s) {
                              ^
error: 150: unknown identifier 's'
    for (; i++ < 100.0; d += (s = 0.001 + abs(s) * 0.7), o += 1.0 / s) {
                                              ^
error: 150: unknown identifier 's'
    for (; i++ < 100.0; d += (s = 0.001 + abs(s) * 0.7), o += 1.0 / s) {
                                                                    ^
error: 151: unknown identifier 's'
... *= float2x2(cos(0.2 * t + p.b * 0.1 + float4(0.0, 33.0, 11.0, 0.0))), p.xy /= sin(p.r + cos(p.g)), s = tanh(1.0 + p.g), n = 2.0;
                                                                                                       ^
error: 151: unknown identifier 'n'
...* t + p.b * 0.1 + float4(0.0, 33.0, 11.0, 0.0))), p.xy /= sin(p.r + cos(p.g)), s = tanh(1.0 + p.g), n = 2.0;
                                                                                                       ^
error: 153: unknown identifier 's'
            s += abs(dot(step(1.0 / d, cos(t + p.b + p * n)), float3(0.4, 0.4, 0.4))) / n;
            ^
error: 166: unknown identifier 's'
    for (; i++ < 70.0; d += (s = 0.001 + abs(min(s, l)) * 0.7), o += 1.0 / s / l) {
                             ^
error: 166: unknown identifier 's'
    for (; i++ < 70.0; d += (s = 0.001 + abs(min(s, l)) * 0.7), o += 1.0 / s / l) {
                                                 ^
error: 166: unknown identifier 'l'
    for (; i++ < 70.0; d += (s = 0.001 + abs(min(s, l)) * 0.7), o += 1.0 / s / l) {
                                                    ^
error: 166: unknown identifier 's'
    for (; i++ < 70.0; d += (s = 0.001 + abs(min(s, l)) * 0.7), o += 1.0 / s / l) {
                                                                           ^
error: 166: unknown identifier 'l'
    for (; i++ < 70.0; d += (s = 0.001 + abs(min(s, l)) * 0.7), o += 1.0 / s / l) {
                                                                               ^
error: 167: unknown identifier 's'
...u * d, d + iTime), p.xy *= float2x2(cos(0.15 * iTime - p.b * 0.15 + float4(0.0, 33.0, 11.0, 0.0))), s = tanh(1.0 - abs(p.r)), l = 0.005 + 0.8 * min(abs(length(p - float3(sin(sin(p.b * 0.3 * 0.5) + iTim...
                                                                                                       ^
error: 167: unknown identifier 'l'
... float2x2(cos(0.15 * iTime - p.b * 0.15 + float4(0.0, 33.0, 11.0, 0.0))), s = tanh(1.0 - abs(p.r)), l = 0.005 + 0.8 * min(abs(length(p - float3(sin(sin(p.b * 0.3 * 0.5) + iTime * 0.7) * 3.0, sin(sin(p....
                                                                                                       ^
error: 167: unknown identifier 'n'
...sin(p.b * 0.2 * 1.3) + iTime * 0.5) * 3.0 + 1.0, 22.0 + iTime + cos(iTime * 0.3) * 16.0)) - 0.3))), n = 2.0;
                                                                                                       ^
error: 169: unknown identifier 's'
            s += abs(dot(round(cos(iTime + p.b + p * n)), float3(0.5, 0.5, 0.5))) / n;
            ^
error: 182: unknown identifier 's'
    for (; i++ < 80.0; o += 1.0 / (s + e * 3.0)) {
                                   ^
error: 182: unknown identifier 'e'
    for (; i++ < 80.0; o += 1.0 / (s + e * 3.0)) {
                                       ^
error: 183: unknown identifier 'e'
        p = float3(u * d, d + T), e = 0.01 + 0.8 * min(abs(0.4 - length(p - float3(sin(sin(p.b * 0.3 * 0.5) + T * 0.7) * 4.0, sin(sin(p...
                                  ^
error: 183: unknown identifier 's'
... * 0.7) * 4.0, sin(sin(p.b * 0.2 * 1.3) + T * 0.5) * 1.23 + 1.0, 9.0 + T + cos(T * 0.5) * 6.0))))), s = 0.001 + abs(1.2 + p.g) * 0.8;
                                                                                                       ^
error: 184: unknown identifier 'w'
        p.g *= 0.5, p.xy -= 1.5, w = 1.0;
                                 ^
error: 185: unknown identifier 'w'
        for (; i++ < 8; w *= l) {
                        ^
error: 185: unknown identifier 'l'
        for (; i++ < 8; w *= l) {
                             ^
error: 186: unknown identifier 'l'
            p *= (l = 3.0 / dot(p = sin(p), p));
                  ^
error: 188: unknown identifier 's'
        d += (s = min(min(e, s), length(p) / w));
              ^
error: 188: unknown identifier 'e'
        d += (s = min(min(e, s), length(p) / w));
                          ^
error: 188: unknown identifier 's'
        d += (s = min(min(e, s), length(p) / w));
                             ^
error: 188: unknown identifier 'w'
        d += (s = min(min(e, s), length(p) / w));
                                             ^
error: 199: unknown identifier 'p'
    for (; i++ < 64.0; o += (1.0 + cos(0.3 * p.b + float4(6.0, 2.0, 3.0, 1.0))) / max(s, 0.01)) {
                                             ^
error: 199: unknown identifier 's'
    for (; i++ < 64.0; o += (1.0 + cos(0.3 * p.b + float4(6.0, 2.0, 3.0, 1.0))) / max(s, 0.01)) {
                                                                                      ^
error: 200: unknown identifier 's'
        d += (s = 0.5 * abs(s) + 0.005), p = float3((u + u - r.xy) / r.g * d, d + t) + 1.0, p.xy *= float2x2(cos(0....
              ^
error: 200: unknown identifier 's'
        d += (s = 0.5 * abs(s) + 0.005), p = float3((u + u - r.xy) / r.g * d, d + t) + 1.0, p.xy *= float2x2(cos(0.3 * t + p.b * ...
                            ^
error: 200: unknown identifier 'p'
        d += (s = 0.5 * abs(s) + 0.005), p = float3((u + u - r.xy) / r.g * d, d + t) + 1.0, p.xy *= float2x2(cos(0.3 * t + p.b * 0.2 + float4(...
                                         ^
error: 200: unknown identifier 'p'
        d += (s = 0.5 * abs(s) + 0.005), p = float3((u + u - r.xy) / r.g * d, d + t) + 1.0, p.xy *= float2x2(cos(0.3 * t + p.b * 0.2 + float4(0.0, 33.0, 11.0, 0.0))), p = tanh(sin(t * 0.3) * 4....
                                                                                            ^
error: 200: unknown identifier 'p'
... abs(s) + 0.005), p = float3((u + u - r.xy) / r.g * d, d + t) + 1.0, p.xy *= float2x2(cos(0.3 * t + p.b * 0.2 + float4(0.0, 33.0, 11.0, 0.0))), p = tanh(sin(t * 0.3) * 4.0) * 2.3 + 11.0 - abs(8.0 - abs...
                                                                                                       ^
error: 200: unknown identifier 'p'
.../ r.g * d, d + t) + 1.0, p.xy *= float2x2(cos(0.3 * t + p.b * 0.2 + float4(0.0, 33.0, 11.0, 0.0))), p = tanh(sin(t * 0.3) * 4.0) * 2.3 + 11.0 - abs(8.0 - abs(2.0 - abs(p))), s = cos(p.r + p.g) + abs(do...
                                                                                                       ^
error: 200: unknown identifier 'p'
...float4(0.0, 33.0, 11.0, 0.0))), p = tanh(sin(t * 0.3) * 4.0) * 2.3 + 11.0 - abs(8.0 - abs(2.0 - abs(p))), s = cos(p.r + p.g) + abs(dot(sin(p * 16.0), 0.1 + p - p));
                                                                                                       ^
error: 200: unknown identifier 's'
...(0.0, 33.0, 11.0, 0.0))), p = tanh(sin(t * 0.3) * 4.0) * 2.3 + 11.0 - abs(8.0 - abs(2.0 - abs(p))), s = cos(p.r + p.g) + abs(dot(sin(p * 16.0), 0.1 + p - p));
                                                                                                       ^
error: 200: unknown identifier 'p'
....0, 11.0, 0.0))), p = tanh(sin(t * 0.3) * 4.0) * 2.3 + 11.0 - abs(8.0 - abs(2.0 - abs(p))), s = cos(p.r + p.g) + abs(dot(sin(p * 16.0), 0.1 + p - p));
                                                                                                       ^
error: 200: unknown identifier 'p'
....0, 0.0))), p = tanh(sin(t * 0.3) * 4.0) * 2.3 + 11.0 - abs(8.0 - abs(2.0 - abs(p))), s = cos(p.r + p.g) + abs(dot(sin(p * 16.0), 0.1 + p - p));
                                                                                                       ^
error: 200: unknown identifier 'p'
...h(sin(t * 0.3) * 4.0) * 2.3 + 11.0 - abs(8.0 - abs(2.0 - abs(p))), s = cos(p.r + p.g) + abs(dot(sin(p * 16.0), 0.1 + p - p));
                                                                                                       ^
error: 200: unknown identifier 'p'
...4.0) * 2.3 + 11.0 - abs(8.0 - abs(2.0 - abs(p))), s = cos(p.r + p.g) + abs(dot(sin(p * 16.0), 0.1 + p - p));
                                                                                                       ^
error: 200: unknown identifier 'p'
... * 2.3 + 11.0 - abs(8.0 - abs(2.0 - abs(p))), s = cos(p.r + p.g) + abs(dot(sin(p * 16.0), 0.1 + p - p));
                                                                                                       ^
error: 254: unknown identifier 'tmp'
    if ((tmp = sdIcosahedron(Spin(t + 7.0, p - float3(-3.75 + 3.0 * sin(q.b), 20.0 * abs(sin(t + 7.0 + q.b)) - 6...
         ^^^
error: 255: unknown identifier 'tmp'
        hit = float2(tmp, 7.0 + floor(iTime / 30.0 / 40.0));
                     ^^^
error: 257: unknown identifier 'tmp'
    if ((tmp = sdTetrahedron(Spin(t + 1.0, p - float3(-2.5 + 3.0 * sin(q.b), 20.0 * abs(sin(t + 1.0 + q.b)) - 6....
         ^^^
error: 258: unknown identifier 'tmp'
        hit = float2(tmp, 1.0 + floor(iTime / 30.0 / 40.0));
                     ^^^
error: 260: unknown identifier 'tmp'
    if ((tmp = sdOctahedron(Spin(t + 2.0, p - float3(1.75 + 3.0 * sin(q.b), 20.0 * abs(sin(t + 2.0 + q.b)) - 6.0...
         ^^^
error: 261: unknown identifier 'tmp'
        hit = float2(tmp, 2.0 + floor(iTime / 30.0 / 40.0));
                     ^^^
error: 263: unknown identifier 'tmp'
    if ((tmp = sdDodecahedron(Spin(t + 3.0, p - float3(-1.75 + 3.0 * sin(q.b), 20.0 * abs(sin(t + 3.0 + q.b)) - ...
         ^^^
error: 264: unknown identifier 'tmp'
        hit = float2(tmp, 3.0 + floor(iTime / 30.0 / 40.0));
                     ^^^
error: 266: unknown identifier 'tmp'
    if ((tmp = sdCube(Spin(t + 4.0, p - float3(3.75 + 3.0 * sin(q.b), 20.0 * abs(sin(t + 4.0 + q.b)) - 6.0, 25.0...
         ^^^
error: 267: unknown identifier 'tmp'
        hit = float2(tmp, 4.0 + floor(iTime / 30.0 / 40.0));
                     ^^^
error: 269: unknown identifier 'tmp'
    if ((tmp = sdSphere(Spin(t + 6.0, p - float3(5.0 + 3.0 * sin(q.b), 20.0 * abs(sin(t + 6.0 + q.b)) - 6.0, 25....
         ^^^
error: 270: unknown identifier 'tmp'
        hit = float2(tmp, 6.0 + floor(iTime / 30.0 / 40.0));
                     ^^^
error: 272: unknown identifier 'tmp'
    if ((tmp = sdTorus(p - float3(-20.0, 30.0 * abs(sin(iTime * 1.6 + q.b)) - 3.5, 30.0).yxz, float2(3.0, 1.0)))...
         ^^^
error: 273: unknown identifier 'tmp'
        hit = float2(tmp, 5.0);
                     ^^^
error: 279: symbol 'light' was already defined
float3 light = float3(-10.0, -10.0, -10.0);
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
error: 298: unknown identifier 't'
    if (t = sdBox(q - float3(24.9, 0.0, 12.0), float3(0.5, 8.5, 10.5)), t < r.r) {
        ^
error: 298: unknown identifier 't'
    if (t = sdBox(q - float3(24.9, 0.0, 12.0), float3(0.5, 8.5, 10.5)), t < r.r) {
                                                                        ^
error: 299: unknown identifier 't'
        r = float2(t, 666.0);
                   ^
error: 307: unknown identifier 't'
    if (t = 550.0 - abs(p.b), t < r.r) {
        ^
error: 307: unknown identifier 't'
    if (t = 550.0 - abs(p.b), t < r.r) {
                              ^
error: 308: unknown identifier 't'
        r = float2(t, 1.0);
                   ^
error: 310: unknown identifier 't'
    if (t = 25.0 + p.g, t < r.r) {
        ^
error: 310: unknown identifier 't'
    if (t = 25.0 + p.g, t < r.r) {
                        ^
error: 311: unknown identifier 't'
        r = float2(t, 552.0);
                   ^
error: 313: unknown identifier 't'
    if (t = 25.0 - p.g, t < r.r) {
        ^
error: 313: unknown identifier 't'
    if (t = 25.0 - p.g, t < r.r) {
                        ^
error: 314: unknown identifier 't'
        r = float2(t, 888.0);
                   ^
error: 316: unknown identifier 't'
    if (t = 25.0 - abs(p.r), t < r.r) {
        ^
error: 316: unknown identifier 't'
    if (t = 25.0 - abs(p.r), t < r.r) {
                             ^
error: 317: unknown identifier 't'
        r = float2(t, 595.0);
                   ^
error: 319: unknown identifier 't'
    if (t = sdSphere(p - light) - 2.0, t < r.r) {
        ^
error: 319: unknown identifier 't'
    if (t = sdSphere(p - light) - 2.0, t < r.r) {
                                       ^
error: 320: unknown identifier 't'
        r = float2(t, 999.0);
                   ^
139 errors
