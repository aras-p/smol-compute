#include "../../source/smolcompute.h"
#include <stdio.h>
#include <string.h>

#define SOKOL_IMPL
#include "external/sokol_time.h"


static bool SmokeTest()
{
    bool ok = false;
    
    SmolBuffer* bufInput = nullptr;
    SmolBuffer* bufOutput = nullptr;
    SmolKernel* cs = nullptr;
    
    const int kInputSize = 1024;
    const int kGroupSize = 16;
    const int kOutputSize = kInputSize / kGroupSize;
    bufInput = SmolBufferCreate(kInputSize*4);
    bufOutput = SmolBufferCreate(kOutputSize*4);
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
    "kernel void kernelFunc(\n"
    "const device uint* bufInput [[buffer(0)]],\n"
    "device uint* bufOutput [[buffer(1)]],\n"
    "uint2 gid [[thread_position_in_grid]])\n"
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
    SmolKernelSetBuffer(bufOutput, 1);
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
    fread(buffer, *outSize, 1, f);
    fclose(f);
    return buffer;
}

static bool IspcCompressBC3Test()
{
    bool ok = false;
    
    const int kImageSize = 512;
    const int kGroupSize = 8;

    size_t inputSize, outputSize, kernelSourceSize;
    void* inputData = nullptr;
    void* outputData = nullptr;
    void* outputDataExpected = nullptr;
    void* kernelSource = nullptr;
    SmolBuffer *bufInput = nullptr, *bufOutput = nullptr, *bufGlobals = nullptr;
    SmolKernel *cs = nullptr;
    uint64_t tStart = 0, tDur = 0;

    inputData = ReadFile("tests/data/ispc-compress-bc3/input512.bin", &inputSize);
    if (inputData == nullptr)
        goto _cleanup;
    outputDataExpected = ReadFile("tests/data/ispc-compress-bc3/output512.bin", &outputSize);
    if (outputDataExpected == nullptr)
        goto _cleanup;
    kernelSource = ReadFile("tests/data/ispc-compress-bc3/kernel.metal", &kernelSourceSize);
    if (kernelSource == nullptr)
        goto _cleanup;
    
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
    
    bufInput = SmolBufferCreate(inputSize+4);
    bufOutput = SmolBufferCreate(outputSize+4);
    bufGlobals = SmolBufferCreate(sizeof(Globals_Type));
    
    Globals_Type glob;
    glob.inputOffset = 0;
    glob.outputOffset = 0;
    glob.image_width = kImageSize;
    glob.image_height = kImageSize;
    glob.width_in_blocks = kImageSize/4;
    glob.height_in_blocks = kImageSize/4;
    
    for (int i = 0; i < 10; ++i)
    {
        tStart = stm_now();
        SmolBufferSetData(bufInput, inputData, inputSize, 4);
        
        SmolBufferSetData(bufGlobals, &glob, sizeof(glob));
        
        SmolKernelSet(cs);
        SmolKernelSetBuffer(bufInput, 2);
        SmolKernelSetBuffer(bufOutput, 0);
        SmolKernelSetBuffer(bufGlobals, 1);
        SmolKernelDispatch(kImageSize, kImageSize, 1, kGroupSize, kGroupSize, 1);
        
        SmolBufferMakeGpuDataVisibleToCpu(bufOutput);
        SmolFinishWork();
        SmolBufferGetData(bufOutput, outputData, outputSize, 4);
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
                printf("    does not match at index %i: %08x vs %0x8\n", i, ptrv, expv);
                ++printed;
            }
        }
        goto _cleanup;
    }
    
    printf("OK: IspcCompressBC3Test passed\n");
    ok = true;

_cleanup:
    SmolBufferDelete(bufInput);
    SmolBufferDelete(bufOutput);
    SmolBufferDelete(bufGlobals);
    SmolKernelDelete(cs);
    if (inputData != nullptr) free(inputData);
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
