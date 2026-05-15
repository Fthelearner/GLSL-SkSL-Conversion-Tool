### Compilation failed:

error: 46: symbol 'hit' was already defined
bool hit = false;
^^^^^^^^^^^^^^^^
error: 73: unknown identifier 'i'
        o *= i;
             ^
error: 76: unknown identifier 'i'
    o *= i;
         ^
error: 78: unknown identifier 's'
        p += D * s, o += (s = map(p));
                 ^
error: 78: unknown identifier 's'
        p += D * s, o += (s = map(p));
                          ^
error: 82: unknown identifier 's'
        p += r * 0.05, D = reflect(D, r), s = (i = 0.0);
                                          ^
error: 82: unknown identifier 'i'
        p += r * 0.05, D = reflect(D, r), s = (i = 0.0);
                                               ^
error: 84: unknown identifier 's'
            p += D * s, s = map(p) * 0.8, ref += s;
                     ^
error: 84: unknown identifier 's'
            p += D * s, s = map(p) * 0.8, ref += s;
                        ^
error: 84: unknown identifier 's'
            p += D * s, s = map(p) * 0.8, ref += s;
                                                 ^
10 errors
