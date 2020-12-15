#include "../../source/smolcompute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "external/sokol_time.h"
#include "external/stb_image.h"
#include "external/stb_image_write.h"
#include "external/rgbcx.h"

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
   
    bufInput = SmolBufferCreate(inputSize, SmolBufferType::Structured, 4);
    bufOutput = SmolBufferCreate(outputSize, SmolBufferType::Structured, 4);
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
