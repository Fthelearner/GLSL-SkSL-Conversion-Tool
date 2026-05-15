### Compilation failed:

error: 8: variables of type 'shader' may not be uniform
uniform shader image;
^^^^^^^^^^^^^^^^^^^^
error: 9: variables of type 'shader' may not be uniform
uniform shader blurMask;
^^^^^^^^^^^^^^^^^^^^^^^
error: 15: type 'shader' has no method named 'eval'
    return image.eval(coord);
           ^^^^^^^^^^
error: 19: type 'shader' has no method named 'eval'
    return blurMask.eval(coord);
           ^^^^^^^^^^^^^
4 errors
