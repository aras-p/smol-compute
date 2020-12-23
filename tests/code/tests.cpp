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
    const char* kernelCode = nullptr;
    size_t kernelCodeSize = 0;
    const SmolBackend backend = SmolComputeGetBackend();

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
    
    if (backend == SmolBackend::D3D11)
    {
        kernelCode = R"(
StructuredBuffer<uint> bufInput : register(t0);
RWStructuredBuffer<uint> bufOutput : register(u1);
[numthreads(16, 1, 1)]
void kernelFunc(uint3 gid : SV_DispatchThreadID)
{
    uint idx = gid.x;
    uint res = 0;
    for (int i = 0; i < 16; ++i)
        res += bufInput[idx*16+i];
    bufOutput[idx] = res;
})";
    }
    else if (backend == SmolBackend::Metal)
    {
        kernelCode = R"(
kernel void kernelFunc(
    const device uint* bufInput [[buffer(0)]],
    device uint* bufOutput [[buffer(1)]],
    uint2 gid [[thread_position_in_grid]])
{
    uint idx = gid.x;
    uint res = 0;
    for (int i = 0; i < 16; ++i)
        res += bufInput[idx*16+i];
    bufOutput[idx] = res;
})";
    }
    else if (backend == SmolBackend::Vulkan)
    {
        // same HLSL shader as above, converted to SPIR-V via shader playground DXC
        uint8_t arr[1112] = {
          0x03,0x02,0x23,0x07,0x00,0x00,0x01,0x00,0x00,0x00,0x0e,0x00,0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
          0x11,0x00,0x02,0x00,0x01,0x00,0x00,0x00,0x0e,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
          0x0f,0x00,0x07,0x00,0x05,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x6b,0x65,0x72,0x6e,0x65,0x6c,0x46,0x75,
          0x6e,0x63,0x00,0x00,0x02,0x00,0x00,0x00,0x10,0x00,0x06,0x00,0x01,0x00,0x00,0x00,0x11,0x00,0x00,0x00,
          0x10,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x03,0x00,0x03,0x00,0x05,0x00,0x00,0x00,
          0x58,0x02,0x00,0x00,0x05,0x00,0x09,0x00,0x03,0x00,0x00,0x00,0x74,0x79,0x70,0x65,0x2e,0x53,0x74,0x72,
          0x75,0x63,0x74,0x75,0x72,0x65,0x64,0x42,0x75,0x66,0x66,0x65,0x72,0x2e,0x75,0x69,0x6e,0x74,0x00,0x00,
          0x05,0x00,0x05,0x00,0x04,0x00,0x00,0x00,0x62,0x75,0x66,0x49,0x6e,0x70,0x75,0x74,0x00,0x00,0x00,0x00,
          0x05,0x00,0x0a,0x00,0x05,0x00,0x00,0x00,0x74,0x79,0x70,0x65,0x2e,0x52,0x57,0x53,0x74,0x72,0x75,0x63,
          0x74,0x75,0x72,0x65,0x64,0x42,0x75,0x66,0x66,0x65,0x72,0x2e,0x75,0x69,0x6e,0x74,0x00,0x00,0x00,0x00,
          0x05,0x00,0x05,0x00,0x06,0x00,0x00,0x00,0x62,0x75,0x66,0x4f,0x75,0x74,0x70,0x75,0x74,0x00,0x00,0x00,
          0x05,0x00,0x05,0x00,0x01,0x00,0x00,0x00,0x6b,0x65,0x72,0x6e,0x65,0x6c,0x46,0x75,0x6e,0x63,0x00,0x00,
          0x47,0x00,0x04,0x00,0x02,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,0x1c,0x00,0x00,0x00,0x47,0x00,0x04,0x00,
          0x04,0x00,0x00,0x00,0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x47,0x00,0x04,0x00,0x04,0x00,0x00,0x00,
          0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x47,0x00,0x04,0x00,0x06,0x00,0x00,0x00,0x22,0x00,0x00,0x00,
          0x00,0x00,0x00,0x00,0x47,0x00,0x04,0x00,0x06,0x00,0x00,0x00,0x21,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
          0x47,0x00,0x04,0x00,0x07,0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x48,0x00,0x05,0x00,
          0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x23,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x48,0x00,0x04,0x00,
          0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x00,0x00,0x00,0x47,0x00,0x03,0x00,0x03,0x00,0x00,0x00,
          0x03,0x00,0x00,0x00,0x48,0x00,0x05,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x23,0x00,0x00,0x00,
          0x00,0x00,0x00,0x00,0x47,0x00,0x03,0x00,0x05,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x15,0x00,0x04,0x00,
          0x08,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x2b,0x00,0x04,0x00,0x08,0x00,0x00,0x00,
          0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x15,0x00,0x04,0x00,0x0a,0x00,0x00,0x00,0x20,0x00,0x00,0x00,
          0x00,0x00,0x00,0x00,0x2b,0x00,0x04,0x00,0x0a,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
          0x2b,0x00,0x04,0x00,0x08,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x2b,0x00,0x04,0x00,
          0x0a,0x00,0x00,0x00,0x0d,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x2b,0x00,0x04,0x00,0x08,0x00,0x00,0x00,
          0x0e,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x1d,0x00,0x03,0x00,0x07,0x00,0x00,0x00,0x0a,0x00,0x00,0x00,
          0x1e,0x00,0x03,0x00,0x03,0x00,0x00,0x00,0x07,0x00,0x00,0x00,0x20,0x00,0x04,0x00,0x0f,0x00,0x00,0x00,
          0x02,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x1e,0x00,0x03,0x00,0x05,0x00,0x00,0x00,0x07,0x00,0x00,0x00,
          0x20,0x00,0x04,0x00,0x10,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x05,0x00,0x00,0x00,0x17,0x00,0x04,0x00,
          0x11,0x00,0x00,0x00,0x0a,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x20,0x00,0x04,0x00,0x12,0x00,0x00,0x00,
          0x01,0x00,0x00,0x00,0x11,0x00,0x00,0x00,0x13,0x00,0x02,0x00,0x13,0x00,0x00,0x00,0x21,0x00,0x03,0x00,
          0x14,0x00,0x00,0x00,0x13,0x00,0x00,0x00,0x14,0x00,0x02,0x00,0x15,0x00,0x00,0x00,0x20,0x00,0x04,0x00,
          0x16,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x0a,0x00,0x00,0x00,0x3b,0x00,0x04,0x00,0x0f,0x00,0x00,0x00,
          0x04,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x3b,0x00,0x04,0x00,0x10,0x00,0x00,0x00,0x06,0x00,0x00,0x00,
          0x02,0x00,0x00,0x00,0x3b,0x00,0x04,0x00,0x12,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
          0x36,0x00,0x05,0x00,0x13,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x14,0x00,0x00,0x00,
          0xf8,0x00,0x02,0x00,0x17,0x00,0x00,0x00,0x3d,0x00,0x04,0x00,0x11,0x00,0x00,0x00,0x18,0x00,0x00,0x00,
          0x02,0x00,0x00,0x00,0x51,0x00,0x05,0x00,0x0a,0x00,0x00,0x00,0x19,0x00,0x00,0x00,0x18,0x00,0x00,0x00,
          0x00,0x00,0x00,0x00,0xf9,0x00,0x02,0x00,0x1a,0x00,0x00,0x00,0xf8,0x00,0x02,0x00,0x1a,0x00,0x00,0x00,
          0xf5,0x00,0x07,0x00,0x0a,0x00,0x00,0x00,0x1b,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,0x17,0x00,0x00,0x00,
          0x1c,0x00,0x00,0x00,0x1d,0x00,0x00,0x00,0xf5,0x00,0x07,0x00,0x08,0x00,0x00,0x00,0x1e,0x00,0x00,0x00,
          0x09,0x00,0x00,0x00,0x17,0x00,0x00,0x00,0x1f,0x00,0x00,0x00,0x1d,0x00,0x00,0x00,0xb1,0x00,0x05,0x00,
          0x15,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x1e,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0xf6,0x00,0x04,0x00,
          0x21,0x00,0x00,0x00,0x1d,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xfa,0x00,0x04,0x00,0x20,0x00,0x00,0x00,
          0x1d,0x00,0x00,0x00,0x21,0x00,0x00,0x00,0xf8,0x00,0x02,0x00,0x1d,0x00,0x00,0x00,0x84,0x00,0x05,0x00,
          0x0a,0x00,0x00,0x00,0x22,0x00,0x00,0x00,0x19,0x00,0x00,0x00,0x0d,0x00,0x00,0x00,0x7c,0x00,0x04,0x00,
          0x0a,0x00,0x00,0x00,0x23,0x00,0x00,0x00,0x1e,0x00,0x00,0x00,0x80,0x00,0x05,0x00,0x0a,0x00,0x00,0x00,
          0x24,0x00,0x00,0x00,0x22,0x00,0x00,0x00,0x23,0x00,0x00,0x00,0x41,0x00,0x06,0x00,0x16,0x00,0x00,0x00,
          0x25,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x09,0x00,0x00,0x00,0x24,0x00,0x00,0x00,0x3d,0x00,0x04,0x00,
          0x0a,0x00,0x00,0x00,0x26,0x00,0x00,0x00,0x25,0x00,0x00,0x00,0x80,0x00,0x05,0x00,0x0a,0x00,0x00,0x00,
          0x1c,0x00,0x00,0x00,0x1b,0x00,0x00,0x00,0x26,0x00,0x00,0x00,0x80,0x00,0x05,0x00,0x08,0x00,0x00,0x00,
          0x1f,0x00,0x00,0x00,0x1e,0x00,0x00,0x00,0x0e,0x00,0x00,0x00,0xf9,0x00,0x02,0x00,0x1a,0x00,0x00,0x00,
          0xf8,0x00,0x02,0x00,0x21,0x00,0x00,0x00,0x41,0x00,0x06,0x00,0x16,0x00,0x00,0x00,0x27,0x00,0x00,0x00,
          0x06,0x00,0x00,0x00,0x09,0x00,0x00,0x00,0x19,0x00,0x00,0x00,0x3e,0x00,0x03,0x00,0x27,0x00,0x00,0x00,
          0x1b,0x00,0x00,0x00,0xfd,0x00,0x01,0x00,0x38,0x00,0x01,0x00 };
        kernelCode = (const char*)arr;
        kernelCodeSize = sizeof(arr);
    }
    if (kernelCodeSize == 0)
        kernelCodeSize = strlen(kernelCode);

    cs = SmolKernelCreate(kernelCode, kernelCodeSize, "kernelFunc");
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
