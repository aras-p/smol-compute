#include "../../source/smolcompute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "external/sokol_time.h"
#include "external/stb_image.h"
#include "external/stb_image_write.h"
#include "external/rgbcx.h"

// ----------------------------------------------------------------------------

// "Reference" CPU implementation of BC3 compressor, to compare with compute shader
// results. Ported over to C from
// https://github.com/GameTechDev/ISPCTextureCompressor/blob/master/ispc_texcomp/kernel.ispc

typedef uint32_t uint;

struct float3
{
    float3() {}
    float3(float v) : r(v), g(v), b(v) {}
    float3(float a, float b, float c) : r(a), g(b), b(c) {}
    float r, g, b;
    
    void operator+=(const float3& o) { r += o.r; g += o.g; b += o.b; }
    void operator*=(float a) { r *= a; g *= a; b *= a; }
    
    const float& operator[](int i) const
    {
        if (i == 0) return r;
        if (i == 1) return g;
        return b;
    }
    float& operator[](int i)
    {
        if (i == 0) return r;
        if (i == 1) return g;
        return b;
    }
};

static inline float3 operator+(const float3& a, const float3& b) { return float3(a.r+b.r, a.g+b.g, a.b+b.b); }
static inline float3 operator-(const float3& a, const float3& b) { return float3(a.r-b.r, a.g-b.g, a.b-b.b); }
static inline float3 operator*(const float3& a, const float3& b) { return float3(a.r*b.r, a.g*b.g, a.b*b.b); }
static inline float3 operator*(float a, const float3& b) { return float3(a*b.r, a*b.g, a*b.b); }
static inline float3 operator*(const float3& a, float b) { return float3(a.r*b, a.g*b, a.b*b); }
static inline float3 operator/(const float3& a, float b) { return float3(a.r/b, a.g/b, a.b/b); }

static inline float dot(const float3& a, const float3& b) { return a.r*b.r + a.g*b.g + a.b*b.b; }
//static inline float3 min(const float3& a, const float3& b) { return float3(std::min(a.r,b.r), std::min(a.g,b.g), std::min(a.b,b.b)); }
//static inline float3 max(const float3& a, const float3& b) { return float3(std::max(a.r,b.r), std::max(a.g,b.g), std::max(a.b,b.b)); }

static inline float3 normalize(const float3& v)
{
    float len = sqrtf(dot(v, v));
    return float3(v.r / len, v.g / len, v.b / len);
}

static inline float3 clamp(const float3& v, float a, float b)
{
    return float3(
                  std::max(std::min(v.r, b), a),
                  std::max(std::min(v.g, b), a),
                  std::max(std::min(v.b, b), a));
}

template<typename T>
static inline T clamp(T v, T a, T b)
{
    return std::max(std::min(v, b), a);
}

struct uint2
{
    uint2() {}
    uint2(uint a, uint b) : x(a), y(b) {}
    
    uint x, y;
    const uint& operator[](int i) const
    {
        if (i == 0) return x;
        return y;
    }
    uint& operator[](int i)
    {
        if (i == 0) return x;
        return y;
    }
};
static inline uint2 operator+(const uint2& a, const uint2 b) { return uint2(a.x+b.x, a.y+b.y); }
static inline uint2 operator<<(const uint2& a, int b) { return uint2(a.x<<b, a.y<<b); }

static inline uint2 min(const uint2& a, const uint2 b) { return uint2(std::min(a.x,b.x), std::min(a.y,b.y)); }


static float3 compute_covar_dc_ugly(float covar[6], const float3 block[16])
{
    int k;
    float3 acc = 0;
    for (k = 0; k < 16; ++k)
        acc += block[k];
    float3 dc = acc / 16;

    float covar0 = 0;
    float covar1 = 0;
    float covar2 = 0;
    float covar3 = 0;
    float covar4 = 0;
    float covar5 = 0;

    for (k=0; k<16; k++)
    {
        float3 rgb;
        rgb.r = block[k].r-dc.r;
        rgb.g = block[k].g-dc.g;
        rgb.b = block[k].b-dc.b;

        covar0 += rgb.r*rgb.r;
        covar1 += rgb.r*rgb.g;
        covar2 += rgb.r*rgb.b;

        covar3 += rgb.g*rgb.g;
        covar4 += rgb.g*rgb.b;

        covar5 += rgb.b*rgb.b;
    }

    covar[0] = covar0;
    covar[1] = covar1;
    covar[2] = covar2;
    covar[3] = covar3;
    covar[4] = covar4;
    covar[5] = covar5;
    return dc;
}

static int stb__Mul8Bit(int a, int b)
{
  int t = a*b + 128;
  return (t + (t >> 8)) >> 8;
}

static uint stb__As16Bit(int r, int g, int b)
{
   return (stb__Mul8Bit(r,31) << 11) + (stb__Mul8Bit(g,63) << 5) + stb__Mul8Bit(b,31);
}

static uint enc_rgb565(float3 c)
{
    return stb__As16Bit((int)c.r, (int)c.g, (int)c.b);
}

static float3 dec_rgb565(int p)
{
    int c2 = (p>>0)&31;
    int c1 = (p>>5)&63;
    int c0 = (p>>11)&31;

    float3 c;
    c.r = (c0<<3)+(c0>>2);
    c.g = (c1<<2)+(c1>>4);
    c.b = (c2<<3)+(c2>>2);
    return c;
}

static void swap_ints(int& u, int& v)
{
    int t = u;
    u = v;
    v = t;
}

static float3 ssymv(const float covar[6], float3 b)
{
    float3 a;
    a.r = covar[0]*b.r+covar[1]*b.g+covar[2]*b.b;
    a.g = covar[1]*b.r+covar[3]*b.g+covar[4]*b.b;
    a.b = covar[2]*b.r+covar[4]*b.g+covar[5]*b.b;
    return a;
}

static void ssymv3(float a[4], const float covar[10], const float b[4])
{
    a[0] = covar[0]*b[0]+covar[1]*b[1]+covar[2]*b[2];
    a[1] = covar[1]*b[0]+covar[4]*b[1]+covar[5]*b[2];
    a[2] = covar[2]*b[0]+covar[5]*b[1]+covar[7]*b[2];
    a[3] = 0;
}

static void ssymv4(float a[4], const float covar[10], const float b[4])
{
    a[0] = covar[0]*b[0]+covar[1]*b[1]+covar[2]*b[2]+covar[3]*b[3];
    a[1] = covar[1]*b[0]+covar[4]*b[1]+covar[5]*b[2]+covar[6]*b[3];
    a[2] = covar[2]*b[0]+covar[5]*b[1]+covar[7]*b[2]+covar[8]*b[3];
    a[3] = covar[3]*b[0]+covar[6]*b[1]+covar[8]*b[2]+covar[9]*b[3];
}

static float3 compute_axis3(float covar[6], const uint powerIterations)
{
    float3 v = float3(1.0f, 2.718281828f, 3.141592654f);
    for (uint i=0; i<powerIterations; i++)
    {
        v = ssymv(covar, v);
        if (i%2==1) // renormalize every other iteration
            v = normalize(v);
    }
    return v;
}

static void compute_axis(float axis[4], const float covar[10], uint powerIterations, int channels)
{
    float vecc[4] = {1,1,1,1};

    for (uint i=0; i<powerIterations; i++)
    {
        if (channels == 3) ssymv3(axis, covar, vecc);
        if (channels == 4) ssymv4(axis, covar, vecc);
        vecc[0] = axis[0]; vecc[1] = axis[1]; vecc[2] = axis[2]; vecc[3] = axis[3];

        if (i%2==1) // renormalize every other iteration
        {
            int p;
            float norm_sq = 0;
            for (p=0; p<channels; p++)
                norm_sq += axis[p]*axis[p];

            float rnorm = 1.0 / sqrt(norm_sq);
            for (p=0; p<channels; p++) vecc[p] *= rnorm;
        }
    }

    axis = vecc;
}

static void pick_endpoints(float3& c0, float3& c1, const float3 block[16], float3 axis, float3 dc)
{
    float min_dot = 256*256;
    float max_dot = 0;

    for (int i = 0; i < 16; ++i)
    {
        float dt = dot(block[i]-dc, axis);
        min_dot = std::min(dt, min_dot);
        max_dot = std::max(dt, max_dot);
    }

    if (max_dot-min_dot < 1)
    {
        min_dot -= 0.5f;
        max_dot += 0.5f;
    }

    float norm_sq = dot(axis, axis);
    float rnorm_sq = 1.0 / norm_sq;
    c0 = clamp(dc+min_dot*rnorm_sq*axis, 0, 255);
    c1 = clamp(dc+max_dot*rnorm_sq*axis, 0, 255);
}

static uint fast_quant(float3 block[16], int p0, int p1)
{
    float3 c0 = dec_rgb565(p0);
    float3 c1 = dec_rgb565(p1);

    float3 dir = c1-c0;
    float sq_norm = dot(dir, dir);
    float rsq_norm = 1.0 / sq_norm;
    dir *= rsq_norm*3;

    float bias = 0.5;
    for (int p=0; p<3; p++) bias -= c0[p]*dir[p];

    uint bits = 0;
    uint scaler = 1;
    for (int k=0; k<16; k++)
    {
        float dt = dot(block[k], dir);
        int q = clamp((int)(dt+bias), 0, 3);

        //bits += q<<(k*2);
        bits += q*scaler;
        scaler *= 4;
    }

    return bits;
}

static void bc1_refine(int pe[2], float3 block[16], uint bits, float3 dc)
{
    float3 c0;
    float3 c1;

    if ((bits ^ (bits*4)) < 4)
    {
        // single color
        c0 = dc;
        c1 = dc;
    }
    else
    {
        float3 Atb1 = float3(0,0,0);
        float sum_q = 0;
        float sum_qq = 0;
        uint shifted_bits = bits;

        for (int k=0; k<16; k++)
        {
            float q = (int)(shifted_bits&3);
            shifted_bits >>= 2;

            float x = 3-q;
            float y = q;

            sum_q += q;
            sum_qq += q*q;

            Atb1 += x*block[k];
        }

        float3 sum = dc*16;
        float3 Atb2 = 3*sum-Atb1;

        float Cxx = 16*3*3-2*3*sum_q+sum_qq;
        float Cyy = sum_qq;
        float Cxy = 3*sum_q-sum_qq;
        float scale = 3 * (1.0 / (Cxx*Cyy - Cxy*Cxy));

        c0 = (Atb1*Cyy - Atb2*Cxy)*scale;
        c1 = (Atb2*Cxx - Atb1*Cxy)*scale;
        c0 = clamp(c0, 0, 255);
        c1 = clamp(c1, 0, 255);
    }

    pe[0] = enc_rgb565(c0);
    pe[1] = enc_rgb565(c1);
}

static uint fix_qbits(uint qbits)
{
    const uint mask_01b = 0x55555555;
    const uint mask_10b = 0xAAAAAAAA;

    uint qbits0 = qbits&mask_01b;
    uint qbits1 = qbits&mask_10b;
    qbits = (qbits1>>1) + (qbits1 ^ (qbits0<<1));

    return qbits;
}

static uint2 CompressBlockBC1_core(float3 block[16])
{
    const int powerIterations = 4;
    const int refineIterations = 1;

    float covar[6];
    float3 dc = compute_covar_dc_ugly(covar, block);

    float eps = 0.001;
    covar[0] += eps;
    covar[3] += eps;
    covar[5] += eps;

    float3 axis = compute_axis3(covar, powerIterations);

    float3 c0;
    float3 c1;
    pick_endpoints(c0, c1, block, axis, dc);
    
    int p[2];
    p[0] = enc_rgb565(c0);
    p[1] = enc_rgb565(c1);
    if (p[0]<p[1]) swap_ints(p[0], p[1]);

    uint2 data;
    data[0] = (1<<16)*p[1]+p[0];
    data[1] = fast_quant(block, p[0], p[1]);

    // refine
    //[unroll]
    for (int i=0; i<refineIterations; i++)
    {
        bc1_refine(p, block, data[1], dc);
        if (p[0]<p[1]) swap_ints(p[0], p[1]);
        data[0] = (1<<16)*p[1]+p[0];
        data[1] = fast_quant(block, p[0], p[1]);
    }

    data[1] = fix_qbits(data[1]);
    return data;
}

static uint2 CompressBlockBC3_alpha(float block[16])
{
    uint k;
    float ep[2] = {255, 0};

    for (k=0; k<16; k++)
    {
        ep[0] = std::min(ep[0], block[k]);
        ep[1] = std::max(ep[1], block[k]);
    }

    if (ep[0] == ep[1]) ep[1] = ep[0]+0.1f;

    uint qblock[2] = {0, 0};
    float scale = 7/(ep[1]-ep[0]);

    for (k=0; k<16; k++)
    {
        float v = block[k];
        float proj = (v-ep[0])*scale+0.5f;

        int q = clamp((int)proj, 0, 7);

        q = 7-q;

        if (q > 0) q++;
        if (q==8) q = 1;

        qblock[k/8] |= q << ((k%8)*3);
    }

    // (could be improved by refinement)

    uint2 data;
    data[0] = clamp((int)ep[0], 0, 255)*256+clamp((int)ep[1], 0, 255);
    data[0] |= qblock[0]<<16;
    data[1] = qblock[0]>>16;
    data[1] |= qblock[1]<<8;
    return data;
}

struct Globals
{
    uint inputOffset;
    uint outputOffset;
    uint image_width;
    uint image_height;
    uint width_in_blocks;
    uint height_in_blocks;
};

static void LoadBlock(uint image_width, uint image_height, const uint* bufInput, uint inputOffset, uint2 id, float3 pixels[16], float alphas[16])
{
    uint2 pixelCoordBase = id << 2;
    for (int i = 0; i < 16; ++i)
    {
        uint2 pixelCoord = pixelCoordBase + uint2(i & 3, i >> 2);
        pixelCoord = min(pixelCoord, uint2(image_width-1, image_height-1));
        uint raw = bufInput[pixelCoord.y*image_width+pixelCoord.x+inputOffset];
        float pixr = (float)(raw & 0xFF);
        float pixg = (float)((raw >> 8) & 0xFF);
        float pixb = (float)((raw >> 16) & 0xFF);
        float pixa = (float)(raw >> 24);
        pixels[i] = float3(pixr, pixg, pixb);
        alphas[i] = pixa;
    }
}

static void computeMain(
    const Globals& globals,
    const uint* bufInput,
    uint* bufOutput,
    uint2 id)
{
    if (id.x >= globals.width_in_blocks || id.y >= globals.height_in_blocks)
        return;
    
    if (id.x == 0 && id.y == 3)
    {
        id.x = 0;
    }
    
    float3 pixels[16];
    float alphas[16];
    LoadBlock(globals.image_width, globals.image_height, bufInput, globals.inputOffset, id, pixels, alphas);

    uint2 resA = CompressBlockBC3_alpha(alphas);
    uint2 res = CompressBlockBC1_core(pixels);
    uint bufIdx = (id.y * globals.width_in_blocks + id.x) * 4 + globals.outputOffset;
    bufOutput[bufIdx + 0] = resA.x;
    bufOutput[bufIdx + 1] = resA.y;
    bufOutput[bufIdx + 2] = res.x;
    bufOutput[bufIdx + 3] = res.y;
}


// ----------------------------------------------------------------------------


static void* ReadFile(const char* path, size_t* outSize)
{
    *outSize = 0;
    FILE* f = fopen(path, "rb");
    if (f == nullptr)
        return nullptr;
    fseek(f, 0, SEEK_END);
    *outSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    void* buffer = malloc(*outSize);
    if (buffer == nullptr)
        return nullptr;
    fread(buffer, *outSize, 1, f);
    fclose(f);
    return buffer;
}


static void store_block_4x4(unsigned char block[16 * 4], int x, int y, int width, int height, unsigned char* rgba)
{
    int storeX = (x + 4 > width) ? width - x : 4;
    int storeY = (y + 4 > height) ? height - y : 4;
    for (int row = 0; row < storeY; ++row)
    {
        unsigned char* dst = rgba + (y + row) * width * 4 + x * 4;
        memcpy(dst, block + row * 4 * 4, storeX * 4);
    }
}

static void decompress_dxtc(int width, int height, bool alpha, const unsigned char* input, unsigned char* rgba)
{
    int blocksX = (width + 3) / 4;
    int blocksY = (height + 3) / 4;
    for (int by = 0; by < blocksY; ++by)
    {
        for (int bx = 0; bx < blocksX; ++bx)
        {
            unsigned char block[16 * 4];
            if (alpha)
            {
                rgbcx::unpack_bc3(input, block);
                input += 16;
            }
            else
            {
                rgbcx::unpack_bc1(input, block, true);
                input += 8;
            }
            store_block_4x4(block, bx * 4, by * 4, width, height, rgba);
        }
    }
}

static void save_bc3_result_image(const char* fn, int width, int height, const void* data)
{
    unsigned char* rgba = new unsigned char[width * height * 4];
    decompress_dxtc(width, height, true, (const unsigned char*)data, rgba);
    stbi_flip_vertically_on_write(1);
    stbi_write_tga(fn, width, height, 4, rgba);
    delete[] rgba;
}


bool IspcCompressBC3Test()
{
    rgbcx::init();
    bool ok = false;

    const int kGroupSize = 8;
#ifdef _MSC_VER
#define kExtension "hlsl"
#else
#define kExtension "metal"
#endif


    size_t inputSize, outputSize, kernelSourceSize;
    void* outputData = nullptr;
    void* outputDataExpected = nullptr;
    void* kernelSource = nullptr;
    SmolBuffer *bufInput = nullptr, *bufOutput = nullptr, *bufGlobals = nullptr;
    SmolKernel *cs = nullptr;
    uint64_t tStart = 0, tDur = 0;

    int inputWidth = 0, inputHeight = 0;
    stbi_set_flip_vertically_on_load(1);
    stbi_uc* inputImage = stbi_load("tests/data/ispc-compress-bc3/16x16.tga", &inputWidth, &inputHeight, nullptr, 4);
    if (inputImage == nullptr)
    {
        printf("ERROR: IspcCompressBC3Test: failed to read input image\n");
        goto _cleanup;
    }
    inputSize = inputWidth * inputHeight * 4;
    outputDataExpected = ReadFile("tests/data/ispc-compress-bc3/16x16_out.bin", &outputSize);
    if (outputDataExpected == nullptr)
    {
        printf("ERROR: IspcCompressBC3Test: failed to read output bytes\n");
        goto _cleanup;
    }
    kernelSource = ReadFile("tests/data/ispc-compress-bc3/kernel." kExtension, &kernelSourceSize);
    if (kernelSource == nullptr)
    {
        printf("ERROR: IspcCompressBC3Test: failed to read shader source\n");
        goto _cleanup;
    }
    
    outputData = malloc(outputSize);
    
    cs = SmolKernelCreate(kernelSource, kernelSourceSize, "computeMain");
    if (cs == nullptr)
    {
        printf("ERROR: IspcCompressBC3Test: failed to create compute shader\n");
        goto _cleanup;
    }
   
    bufInput = SmolBufferCreate(inputSize, SmolBufferType::Structured, 4);
    bufOutput = SmolBufferCreate(outputSize, SmolBufferType::Structured, 4);
    bufGlobals = SmolBufferCreate(sizeof(Globals), SmolBufferType::Constant);
    
    Globals glob;
    glob.inputOffset = 0;
    glob.outputOffset = 0;
    glob.image_width = inputWidth;
    glob.image_height = inputHeight;
    glob.width_in_blocks = inputWidth / 4;
    glob.height_in_blocks = inputHeight / 4;
    
    for (int i = 0; i < 5; ++i)
    {
        tStart = stm_now();
        SmolBufferSetData(bufInput, inputImage, inputSize, 0);
        
        SmolBufferSetData(bufGlobals, &glob, sizeof(glob));
        
        SmolKernelSet(cs);
        SmolKernelSetBuffer(bufInput, 2, SmolBufferBinding::Input);
        SmolKernelSetBuffer(bufOutput, 0, SmolBufferBinding::Output);
        SmolKernelSetBuffer(bufGlobals, 1, SmolBufferBinding::Constant);
        SmolKernelDispatch(inputWidth, inputHeight, 1, kGroupSize, kGroupSize, 1);
        
        SmolBufferMakeGpuDataVisibleToCpu(bufOutput);
        SmolFinishWork();
        SmolBufferGetData(bufOutput, outputData, outputSize, 0);
        tDur = stm_since(tStart);
        printf("  BC3 set+compress+get for %ix%i took %.1fms\n", glob.image_width, glob.image_height, stm_ms(tDur));
    }
    
    // CPU eval test
    /*
    for (int by = 0; by < glob.height_in_blocks; ++by)
    {
        for (int bx = 0; bx < glob.width_in_blocks; ++bx)
        {
            computeMain(glob, (const uint*)inputImage, (uint*)outputData, uint2(bx, by));
        }
    }
     */

    if (memcmp(outputData, outputDataExpected, outputSize) != 0)
    {
        printf("ERROR: IspcCompressBC3Test: compute shader did not produce expected data\n");
        const uint32_t* ptr = (const uint32_t*)outputData;
        const uint32_t* exp = (const uint32_t*)outputDataExpected;
        int printed = 0;
        for (int i = 0; i < outputSize/4 && printed < 100; ++i)
        {
            uint32_t ptrv = ptr[i], expv = exp[i];
            if (ptrv != expv)
            {
                printf("    does not match at index %i: got %08x exp %08x\n", i, ptrv, expv);
                ++printed;
            }
        }
        printf("  %i words mismatch\n", printed);
        const char* expTga = "tests/data/ispc-compress-bc3/16x16_exp.tga";
        const char* gotTga = "tests/data/ispc-compress-bc3/16x16_got.tga";
        save_bc3_result_image(expTga, inputWidth, inputHeight, outputDataExpected);
        save_bc3_result_image(gotTga, inputWidth, inputHeight, outputData);
        printf("  images written to %s and %s\n", expTga, gotTga);
        goto _cleanup;
    }
    
    printf("OK: IspcCompressBC3Test passed\n");
    ok = true;

_cleanup:
    SmolBufferDelete(bufInput);
    SmolBufferDelete(bufOutput);
    SmolBufferDelete(bufGlobals);
    SmolKernelDelete(cs);
    if (inputImage != nullptr) stbi_image_free(inputImage);
    if (outputData != nullptr) free(outputData);
    if (outputDataExpected != nullptr) free(outputDataExpected);
    return ok;
}
