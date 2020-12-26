# smol-compute: a small library for compute shader stuffs

A tiny library that allows creating some data buffers and launching compute shaders
on the GPU. With D3D11, Metal and Vulkan implementations at the moment.

**Very very work in progress**, you really don't want to use it for anything yet.


### License

License for the library itself is MIT.

However, the test source code includes several external library source files (all under `tests/code/external`), each with their own license:

* `sokol_time.h` from [floooh/sokol](https://github.com/floooh/sokol): zlib/libpng,
* `stb_image.h` from [nothings/stb](https://github.com/nothings/stb/blob/master/stb_image.h): MIT or public domain,
* manually ported parts of [ISPCTextureCompressor](https://github.com/GameTechDev/ISPCTextureCompressor) for some test compute kernels: MIT.
