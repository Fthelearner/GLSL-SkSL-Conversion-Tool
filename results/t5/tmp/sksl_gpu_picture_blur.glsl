### Compilation failed:

error: 10: variables of type 'shader' may not be uniform
uniform shader iChannel0;
^^^^^^^^^^^^^^^^^^^^^^^^
error: 32: type 'shader' has no method named 'eval'
                fragColor += iChannel0.eval(offset + lens);
                             ^^^^^^^^^^^^^^
error: 39: type 'shader' has no method named 'eval'
        fragColor = mix(iChannel0.eval(uv), lighting, transition);
                        ^^^^^^^^^^^^^^
error: 41: type 'shader' has no method named 'eval'
        fragColor = iChannel0.eval(uv);
                    ^^^^^^^^^^^^^^
4 errors
