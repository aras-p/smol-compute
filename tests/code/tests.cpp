#include "../../source/smolcompute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

bool IspcCompressBC3Test();

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
