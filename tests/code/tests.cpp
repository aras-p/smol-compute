#include "../../source/smolcompute.h"
#include <stdio.h>
#include <string.h>


int main()
{
    if (!SmolComputeCreate())
    {
        printf("ERROR: failed to initialize graphics\n");
        return 1;
    }
        
    int errors = 0;
    
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
        printf("ERROR: buffer data set followed by get did not return the same data\n");
        errors = 1;
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
        printf("ERROR: failed to create compute shader\n");
        errors = 1;
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
        printf("ERROR: compute shader did not produce expected data\n");
        errors = 1;
        goto _cleanup;
    }

_cleanup:
    SmolBufferDelete(bufInput);
    SmolBufferDelete(bufOutput);
    SmolKernelDelete(cs);
    SmolComputeDelete();
    
    if (errors == 0)
        printf("All OK!\n");
    return errors;
}
