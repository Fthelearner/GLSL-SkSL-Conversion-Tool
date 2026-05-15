### Compilation failed:

error: 8: variables of type 'shader' may not be uniform
uniform shader image;
^^^^^^^^^^^^^^^^^^^^
error: 9: variables of type 'shader' may not be uniform
uniform shader preblurImage;
^^^^^^^^^^^^^^^^^^^^^^^^^^^
error: 18: type 'shader' has no method named 'eval'
    return image.eval(coord);
           ^^^^^^^^^^
error: 22: type 'shader' has no method named 'eval'
    return preblurImage.eval(coord);
           ^^^^^^^^^^^^^^^^^
4 errors
