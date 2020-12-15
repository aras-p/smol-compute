#include "../../source/smolcompute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "external/sokol_time.h"
#include "external/stb_image.h"
#include "external/stb_image_write.h"
#include "external/rgbcx.h"

static bool SmokeTest()
{
    bool ok = false;
    
    SmolBuffer* bufInput = nullptr;
    SmolBuffer* bufOutput = nullptr;
    SmolKernel* cs = nullptr;
    
    const int kInputSize = 1024;
    const int kGroupSize = 16;
    const int kOutputSize = kInputSize / kGroupSize;
    bufInput = SmolBufferCreate(kInputSize*4, SmolBufferType::Structured, 4);
    bufOutput = SmolBufferCreate(kOutputSize*4, SmolBufferType::Structured, 4);
    int input[kInputSize];
    for (int i = 0; i < kInputSize; ++i)
        input[i] = i * 17;
    
    // set input data in two parts just to check if that works
    SmolBufferSetData(bufInput, input, kInputSize/3*4);
    SmolBufferSetData(bufInput, input+kInputSize/3, (kInputSize-kInputSize/3)*4, kInputSize/3*4);
    
    // get input data, check if it was fine
    int inputCheck[kInputSize];
    SmolBufferGetData(bufInput, inputCheck, kInputSize*4);
    if (memcmp(input, inputCheck, sizeof(input)) != 0)
    {
        printf("ERROR: SmokeTest: buffer data set followed by get did not return the same data\n");
        goto _cleanup;
    }
    
    const char* kKernelCode;
    kKernelCode =
#ifdef _MSC_VER
        "StructuredBuffer<uint> bufInput : register(t0);\n"
        "RWStructuredBuffer<uint> bufOutput : register(u1);\n"
        "[numthreads(16, 1, 1)]\n"
        "void kernelFunc(uint3 gid : SV_DispatchThreadID)\n"
#else
        "kernel void kernelFunc(\n"
        "const device uint* bufInput [[buffer(0)]],\n"
        "device uint* bufOutput [[buffer(1)]],\n"
        "uint2 gid [[thread_position_in_grid]])\n"
#endif
        "{\n"
        "    uint idx = gid.x;\n"
        "    uint res = 0;\n"
        "    for (int i = 0; i < 16; ++i)\n"
        "        res += bufInput[idx*16+i];\n"
        "    bufOutput[idx] = res;\n"
        "}\n";
    cs = SmolKernelCreate(kKernelCode, strlen(kKernelCode), "kernelFunc");
    if (cs == nullptr)
    {
        printf("ERROR: SmokeTest: failed to create compute shader\n");
        goto _cleanup;
    }
    
    SmolKernelSet(cs);
    SmolKernelSetBuffer(bufInput, 0);
    SmolKernelSetBuffer(bufOutput, 1, SmolBufferBinding::Output);
    SmolKernelDispatch(kInputSize, 1, 1, kGroupSize, 1, 1);
    
    int outputCheck[kOutputSize];
    for (int i = 0; i < kOutputSize; ++i)
    {
        int res = 0;
        for (int j = 0; j < kGroupSize; ++j)
            res += input[i*kGroupSize+j];
        outputCheck[i] = res;
    }
    int output[kOutputSize];
    SmolBufferMakeGpuDataVisibleToCpu(bufOutput);
    SmolFinishWork();
    SmolBufferGetData(bufOutput, output, kOutputSize*4);
    
    if (memcmp(output, outputCheck, sizeof(output)) != 0)
    {
        printf("ERROR: SmokeTest: compute shader did not produce expected data\n");
        goto _cleanup;
    }
    
    printf("OK: SmokeTest passed\n");
    ok = true;

_cleanup:
    SmolBufferDelete(bufInput);
    SmolBufferDelete(bufOutput);
    SmolKernelDelete(cs);
    return ok;
}

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


static bool IspcCompressBC3Test()
{
    rgbcx::init();
    bool ok = false;

    const int kGroupSize = 8;
#ifdef _MSC_VER
    const int kStartSpace = 0;
#define kExtension "hlsl"
#else
    const int kStartSpace = 4;
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
    
    struct Globals_Type
    {
        uint32_t inputOffset;
        uint32_t outputOffset;
        uint32_t image_width;
        uint32_t image_height;
        uint32_t width_in_blocks;
        uint32_t height_in_blocks;
    };
    
    outputData = malloc(outputSize);
    
    cs = SmolKernelCreate(kernelSource, kernelSourceSize, "computeMain");
    if (cs == nullptr)
    {
        printf("ERROR: IspcCompressBC3Test: failed to create compute shader\n");
        goto _cleanup;
    }
   
    bufInput = SmolBufferCreate(inputSize + kStartSpace, SmolBufferType::Structured, 4);
    bufOutput = SmolBufferCreate(outputSize + kStartSpace, SmolBufferType::Structured, 4);
    bufGlobals = SmolBufferCreate(sizeof(Globals_Type), SmolBufferType::Constant);
    
    Globals_Type glob;
    glob.inputOffset = 0;
    glob.outputOffset = 0;
    glob.image_width = inputWidth;
    glob.image_height = inputHeight;
    glob.width_in_blocks = inputWidth / 4;
    glob.height_in_blocks = inputHeight / 4;
    
    for (int i = 0; i < 15; ++i)
    {
        tStart = stm_now();
        SmolBufferSetData(bufInput, inputImage, inputSize, kStartSpace);
        
        SmolBufferSetData(bufGlobals, &glob, sizeof(glob));
        
        SmolKernelSet(cs);
        SmolKernelSetBuffer(bufInput, 2, SmolBufferBinding::Input);
        SmolKernelSetBuffer(bufOutput, 0, SmolBufferBinding::Output);
        SmolKernelSetBuffer(bufGlobals, 1, SmolBufferBinding::Constant);
        SmolKernelDispatch(inputWidth, inputHeight, 1, kGroupSize, kGroupSize, 1);
        
        SmolBufferMakeGpuDataVisibleToCpu(bufOutput);
        SmolFinishWork();
        SmolBufferGetData(bufOutput, outputData, outputSize, kStartSpace);
        tDur = stm_since(tStart);
        printf("  BC3 set+compress+get for %ix%i took %.1fms\n", glob.image_width, glob.image_height, stm_ms(tDur));
    }

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

int main()
{
    stm_setup();
    uint64_t tStart = stm_now(), tDur = 0;
    if (!SmolComputeCreate())
    {
        printf("ERROR: failed to initialize smol_compute\n");
        return 1;
    }

    bool ok = false;
    if (!SmokeTest())
        goto _cleanup;
    if (!IspcCompressBC3Test())
    {
        printf("ERROR: IspcCompressBC3Test: failed\n");
        goto _cleanup;
    }

    ok = true;
    tDur = stm_since(tStart);
    printf("All good! Tests ran for %.3fs\n", stm_sec(tDur));

_cleanup:
    SmolComputeDelete();
    return ok ? 0 : 1;
}
