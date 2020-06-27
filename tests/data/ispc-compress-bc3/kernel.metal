#include <metal_stdlib>
#include <metal_texture>
using namespace metal;

#if !(__HAVE_FMA__)
#define fma(a,b,c) ((a) * (b) + (c))
#endif

constant float4 ImmCB_0[4] =
{
    float4(1.0, 0.0, 0.0, 0.0),
    float4(0.0, 1.0, 0.0, 0.0),
    float4(0.0, 0.0, 1.0, 0.0),
    float4(0.0, 0.0, 0.0, 1.0)
};
struct Globals_Type
{
    uint inputOffset;
    uint outputOffset;
    uint image_width;
    uint image_height;
    uint width_in_blocks;
    uint height_in_blocks;
};

struct bufInput_Type
{
    uint value[1];
};

struct bufOutput_Type
{
    uint value[1];
};

template <typename UVecType> UVecType bitFieldInsert(const UVecType width, const UVecType offset, const UVecType src2, const UVecType src3)
{
    UVecType bitmask = (((UVecType(1) << width)-1) << offset) & 0xffffffff;
    return ((src2 << offset) & bitmask) | (src3 & ~bitmask);
};
template <int N> vec<uint, N> bitFieldExtractU(const vec<uint, N> width, const vec<uint, N> offset, const vec<uint, N> src)
{
    vec<bool, N> isWidthZero = (width == 0);
    vec<bool, N> needsClamp = ((width + offset) < 32);
    vec<uint, N> clampVersion = src << (32-(width+offset));
    clampVersion = clampVersion >> (32 - width);
    vec<uint, N> simpleVersion = src >> offset;
    vec<uint, N> res = select(simpleVersion, clampVersion, needsClamp);
    return select(res, vec<uint, N>(0), isWidthZero);
};
kernel void computeMain(
    constant Globals_Type& Globals [[ buffer(1) ]],
    const device bufInput_Type *bufInput [[ buffer(2) ]],
    device bufOutput_Type *bufOutput [[ buffer(0) ]],
    uint3 mtl_ThreadID [[ thread_position_in_grid ]])
{
    bufInput = reinterpret_cast<const device bufInput_Type *> (reinterpret_cast<device const atomic_uint *> (bufInput) + 1);
    bufOutput = reinterpret_cast<device bufOutput_Type *> (reinterpret_cast<device atomic_uint *> (bufOutput) + 1);
    float3 u_xlat0;
    int2 u_xlati0;
    uint2 u_xlatu0;
    bool2 u_xlatb0;
    float3 u_xlat1;
    int3 u_xlati1;
    float3 u_xlat2;
    int2 u_xlati2;
    float3 u_xlat3;
    float3 u_xlat4;
    float3 u_xlat5;
    float3 u_xlat6;
    float3 u_xlat7;
    float3 u_xlat8;
    float3 u_xlat9;
    float3 u_xlat10;
    float3 u_xlat11;
    float3 u_xlat12;
    float3 u_xlat13;
    float3 u_xlat14;
    float3 u_xlat15;
    float4 u_xlat16;
    int3 u_xlati16;
    float3 u_xlat17;
    int3 u_xlati17;
    float3 u_xlat18;
    int3 u_xlati18;
    float4 u_xlat19;
    float3 u_xlat20;
    float3 u_xlat21;
    int u_xlati22;
    uint u_xlatu22;
    float u_xlat23;
    int u_xlati23;
    uint2 u_xlatu23;
    bool u_xlatb23;
    float3 u_xlat24;
    int u_xlati24;
    bool u_xlatb24;
    int u_xlati44;
    uint2 u_xlatu44;
    int u_xlati45;
    uint2 u_xlatu45;
    float u_xlat46;
    bool u_xlatb46;
    float u_xlat66;
    int u_xlati66;
    uint u_xlatu66;
    float u_xlat67;
    int u_xlati67;
    uint u_xlatu67;
    bool u_xlatb67;
    float u_xlat68;
    int u_xlati68;
    uint u_xlatu68;
    bool u_xlatb68;
    float u_xlat69;
    int u_xlati69;
    bool u_xlatb69;
    float u_xlat70;
    int u_xlati70;
    uint u_xlatu70;
    bool u_xlatb70;
    float u_xlat71;
    int u_xlati71;
    float4 TempArray0[16];
    float4 TempArray1[16];
    float4 TempArray2[2];
    float4 TempArray3[2];
    float4 TempArray4[16];
    float4 TempArray5[16];
    float4 TempArray6[2];
    float4 TempArray7[16];
    u_xlatb0.xy = (mtl_ThreadID.xy>=uint2(Globals.width_in_blocks, Globals.height_in_blocks));
    u_xlatb0.x = u_xlatb0.y || u_xlatb0.x;
    if(u_xlatb0.x){
        return;
    }
    u_xlati0.xy = int2(mtl_ThreadID.xy) << int2(0x2, 0x2);
    u_xlatu44.xy = uint2(Globals.image_width, Globals.image_height) + uint2(0xffffffffu, 0xffffffffu);
    u_xlati1.x = 0x0;
    while(true){
        u_xlatb23 = u_xlati1.x>=0x10;
        if(u_xlatb23){break;}
        u_xlati2.x = int(uint(u_xlati1.x) & 0x3u);
        u_xlati2.y = u_xlati1.x >> 0x2;
        u_xlatu23.xy = uint2(u_xlati0.xy) + uint2(u_xlati2.xy);
        u_xlatu23.xy = min(u_xlatu44.xy, u_xlatu23.xy);
        u_xlati23 = int(u_xlatu23.y) * int(Globals.image_width) + int(u_xlatu23.x);
        u_xlati23 = u_xlati23 + int(Globals.inputOffset);
        u_xlatu23.x = bufInput[u_xlati23].value[(0x0 >> 2) + 0];
        u_xlatu45.x = u_xlatu23.x & 0xffu;
        u_xlat2.x = float(u_xlatu45.x);
        u_xlatu45.xy = bitFieldExtractU(uint2(0x8u, 0x8u), uint2(0x8u, 0x10u), u_xlatu23.xx);
        u_xlat2.yz = float2(u_xlatu45.xy);
        u_xlatu23.x = u_xlatu23.x >> 0x18u;
        u_xlat23 = float(u_xlatu23.x);
        TempArray0[u_xlati1.x].xyz = u_xlat2.xyz;
        TempArray1[u_xlati1.x].x = u_xlat23;
        u_xlati1.x = u_xlati1.x + 0x1;
    }
    u_xlat0.xyz = TempArray0[0].xyz;
    u_xlat1.xyz = TempArray0[1].xyz;
    u_xlat2.xyz = TempArray0[2].xyz;
    u_xlat3.xyz = TempArray0[3].xyz;
    u_xlat4.xyz = TempArray0[4].xyz;
    u_xlat5.xyz = TempArray0[5].xyz;
    u_xlat6.xyz = TempArray0[6].xyz;
    u_xlat7.xyz = TempArray0[7].xyz;
    u_xlat8.xyz = TempArray0[8].xyz;
    u_xlat9.xyz = TempArray0[9].xyz;
    u_xlat10.xyz = TempArray0[10].xyz;
    u_xlat11.xyz = TempArray0[11].xyz;
    u_xlat12.xyz = TempArray0[12].xyz;
    u_xlat13.xyz = TempArray0[13].xyz;
    u_xlat14.xyz = TempArray0[14].xyz;
    u_xlat15.xyz = TempArray0[15].xyz;
    TempArray2[0].x = 255.0;
    TempArray2[1].x = 0.0;
    u_xlatu66 = 0x0u;
    while(true){
        u_xlatb67 = u_xlatu66>=0x10u;
        if(u_xlatb67){break;}
        u_xlat67 = TempArray2[0].x;
        u_xlat68 = TempArray1[int(u_xlatu66)].x;
        u_xlat67 = min(u_xlat67, u_xlat68);
        TempArray2[0].x = u_xlat67;
        u_xlat67 = TempArray2[1].x;
        u_xlat67 = max(u_xlat68, u_xlat67);
        TempArray2[1].x = u_xlat67;
        u_xlatu66 = u_xlatu66 + 0x1u;
    }
    u_xlat66 = TempArray2[0].x;
    u_xlat67 = TempArray2[1].x;
    u_xlatb67 = u_xlat66==u_xlat67;
    if(u_xlatb67){
        u_xlat66 = u_xlat66 + 0.100000001;
        TempArray2[1].x = u_xlat66;
    }
    TempArray3[0].x = 0.0;
    TempArray3[1].x = 0.0;
    u_xlat66 = TempArray2[1].x;
    u_xlat67 = TempArray2[0].x;
    u_xlat66 = u_xlat66 + (-u_xlat67);
    u_xlat66 = 7.0 / u_xlat66;
    u_xlatu68 = 0x0u;
    while(true){
        u_xlatb69 = u_xlatu68>=0x10u;
        if(u_xlatb69){break;}
        u_xlat69 = TempArray1[int(u_xlatu68)].x;
        u_xlat69 = (-u_xlat67) + u_xlat69;
        u_xlat69 = fma(u_xlat69, u_xlat66, 0.5);
        u_xlati69 = int(u_xlat69);
        u_xlati69 = max(u_xlati69, 0x0);
        u_xlati69 = min(u_xlati69, 0x7);
        u_xlati70 = (-u_xlati69) + 0x7;
        u_xlatb70 = 0x0<u_xlati70;
        if(u_xlatb70){
            u_xlati69 = (-u_xlati69) + 0x8;
        } else {
            u_xlati69 = 0x0;
        }
        u_xlatb70 = u_xlati69==0x8;
        if(u_xlatb70){
            u_xlati69 = 0x1;
        }
        u_xlatu70 = u_xlatu68 >> 0x3u;
        u_xlati71 = int(u_xlatu68 & 0x7u);
        u_xlati71 = u_xlati71 * 0x3;
        u_xlati69 = u_xlati69 << u_xlati71;
        u_xlat71 = TempArray3[int(u_xlatu70)].x;
        u_xlat69 = as_type<float>(uint(u_xlati69) | as_type<uint>(u_xlat71));
        TempArray3[int(u_xlatu70)].x = u_xlat69;
        u_xlatu68 = u_xlatu68 + 0x1u;
    }
    u_xlat66 = TempArray2[0].x;
    u_xlati66 = int(u_xlat66);
    u_xlati66 = max(u_xlati66, 0x0);
    u_xlati66 = min(u_xlati66, 0xff);
    u_xlat67 = TempArray2[1].x;
    u_xlati67 = int(u_xlat67);
    u_xlati67 = max(u_xlati67, 0x0);
    u_xlati67 = min(u_xlati67, 0xff);
    u_xlati66 = u_xlati66 * 0x100 + u_xlati67;
    u_xlat67 = TempArray3[0].x;
    u_xlati66 = int(bitFieldInsert(0x10u, 0x10u, as_type<uint>(u_xlat67), uint(u_xlati66)));
    u_xlatu67 = as_type<uint>(u_xlat67) >> 0x10u;
    u_xlat68 = TempArray3[1].x;
    u_xlati68 = as_type<int>(u_xlat68) << 0x8;
    u_xlati67 = int(u_xlatu67 | uint(u_xlati68));
    TempArray4[0].xyz = u_xlat0.xyz;
    TempArray4[1].xyz = u_xlat1.xyz;
    TempArray4[2].xyz = u_xlat2.xyz;
    TempArray4[3].xyz = u_xlat3.xyz;
    TempArray4[4].xyz = u_xlat4.xyz;
    TempArray4[5].xyz = u_xlat5.xyz;
    TempArray4[6].xyz = u_xlat6.xyz;
    TempArray4[7].xyz = u_xlat7.xyz;
    TempArray4[8].xyz = u_xlat8.xyz;
    TempArray4[9].xyz = u_xlat9.xyz;
    TempArray4[10].xyz = u_xlat10.xyz;
    TempArray4[11].xyz = u_xlat11.xyz;
    TempArray4[12].xyz = u_xlat12.xyz;
    TempArray4[13].xyz = u_xlat13.xyz;
    TempArray4[14].xyz = u_xlat14.xyz;
    TempArray4[15].xyz = u_xlat15.xyz;
    u_xlat16.x = float(0.0);
    u_xlat16.y = float(0.0);
    u_xlat16.z = float(0.0);
    u_xlati68 = 0x0;
    while(true){
        u_xlatb69 = u_xlati68>=0x10;
        if(u_xlatb69){break;}
        u_xlat17.xyz = TempArray4[u_xlati68].xyz;
        u_xlat16.xyz = u_xlat16.xyz + u_xlat17.xyz;
        u_xlati68 = u_xlati68 + 0x1;
    }
    u_xlat17.xyz = u_xlat16.xyz * float3(0.0625, 0.0625, 0.0625);
    u_xlat18.x = float(0.0);
    u_xlat18.y = float(0.0);
    u_xlat18.z = float(0.0);
    u_xlat19.x = float(0.0);
    u_xlat19.y = float(0.0);
    u_xlat19.z = float(0.0);
    u_xlati68 = 0x0;
    while(true){
        u_xlatb69 = u_xlati68>=0x10;
        if(u_xlatb69){break;}
        u_xlat69 = TempArray4[u_xlati68].x;
        u_xlat69 = fma((-u_xlat16.x), 0.0625, u_xlat69);
        u_xlat70 = TempArray4[u_xlati68].y;
        u_xlat70 = fma((-u_xlat16.y), 0.0625, u_xlat70);
        u_xlat71 = TempArray4[u_xlati68].z;
        u_xlat71 = fma((-u_xlat16.z), 0.0625, u_xlat71);
        u_xlat18.x = fma(u_xlat69, u_xlat69, u_xlat18.x);
        u_xlat19.x = fma(u_xlat69, u_xlat70, u_xlat19.x);
        u_xlat19.y = fma(u_xlat69, u_xlat71, u_xlat19.y);
        u_xlat18.y = fma(u_xlat70, u_xlat70, u_xlat18.y);
        u_xlat19.z = fma(u_xlat70, u_xlat71, u_xlat19.z);
        u_xlat18.z = fma(u_xlat71, u_xlat71, u_xlat18.z);
        u_xlati68 = u_xlati68 + 0x1;
    }
    u_xlat19 = u_xlat19.xyxz;
    u_xlat18.xyz = u_xlat18.xyz + float3(0.00100000005, 0.00100000005, 0.00100000005);
    u_xlat20.x = float(1.0);
    u_xlat20.y = float(1.0);
    u_xlat20.z = float(1.0);
    u_xlatu68 = 0x0u;
    while(true){
        u_xlatb69 = u_xlatu68>=0x4u;
        if(u_xlatb69){break;}
        u_xlat21.xy = u_xlat19.xz * u_xlat20.yx;
        u_xlat21.xy = fma(u_xlat18.xy, u_xlat20.xy, u_xlat21.xy);
        u_xlat21.xy = fma(u_xlat19.yw, u_xlat20.zz, u_xlat21.xy);
        u_xlat69 = dot(u_xlat19.yw, u_xlat20.xy);
        u_xlat21.z = fma(u_xlat18.z, u_xlat20.z, u_xlat69);
        u_xlati69 = int(u_xlatu68 & 0x1u);
        u_xlatb69 = u_xlati69==0x1;
        if(u_xlatb69){
            u_xlat69 = dot(u_xlat21.xyz, u_xlat21.xyz);
            u_xlat69 = rsqrt(u_xlat69);
            u_xlat20.xyz = float3(u_xlat69) * u_xlat21.xyz;
        } else {
            u_xlat20.xyz = u_xlat21.xyz;
        }
        u_xlatu68 = u_xlatu68 + 0x1u;
    }
    TempArray5[0].xyz = u_xlat0.xyz;
    TempArray5[1].xyz = u_xlat1.xyz;
    TempArray5[2].xyz = u_xlat2.xyz;
    TempArray5[3].xyz = u_xlat3.xyz;
    TempArray5[4].xyz = u_xlat4.xyz;
    TempArray5[5].xyz = u_xlat5.xyz;
    TempArray5[6].xyz = u_xlat6.xyz;
    TempArray5[7].xyz = u_xlat7.xyz;
    TempArray5[8].xyz = u_xlat8.xyz;
    TempArray5[9].xyz = u_xlat9.xyz;
    TempArray5[10].xyz = u_xlat10.xyz;
    TempArray5[11].xyz = u_xlat11.xyz;
    TempArray5[12].xyz = u_xlat12.xyz;
    TempArray5[13].xyz = u_xlat13.xyz;
    TempArray5[14].xyz = u_xlat14.xyz;
    TempArray5[15].xyz = u_xlat15.xyz;
    u_xlat18.x = float(65536.0);
    u_xlat18.y = float(0.0);
    u_xlati68 = 0x0;
    while(true){
        u_xlatb69 = u_xlati68>=0x10;
        if(u_xlatb69){break;}
        u_xlat19.xyz = TempArray5[u_xlati68].xyz;
        u_xlat19.xyz = fma((-u_xlat16.xyz), float3(0.0625, 0.0625, 0.0625), u_xlat19.xyz);
        u_xlat69 = dot(u_xlat19.xyz, u_xlat20.xyz);
        u_xlat18.x = min(u_xlat18.x, u_xlat69);
        u_xlat18.y = max(u_xlat18.y, u_xlat69);
        u_xlati68 = u_xlati68 + 0x1;
    }
    u_xlat68 = (-u_xlat18.x) + u_xlat18.y;
    u_xlatb68 = u_xlat68<1.0;
    if(u_xlatb68){
        u_xlat18.xy = u_xlat18.xy + float2(-0.5, 0.5);
    }
    u_xlat68 = dot(u_xlat20.xyz, u_xlat20.xyz);
    u_xlat68 = float(1.0) / float(u_xlat68);
    u_xlat16.xy = float2(u_xlat68) * u_xlat18.xy;
    u_xlat16.xzw = fma(u_xlat16.xxx, u_xlat20.xyz, u_xlat17.xyz);
    u_xlat16.xzw = max(u_xlat16.xzw, float3(0.0, 0.0, 0.0));
    u_xlat16.xzw = min(u_xlat16.xzw, float3(255.0, 255.0, 255.0));
    u_xlat17.xyz = fma(u_xlat16.yyy, u_xlat20.xyz, u_xlat17.xyz);
    u_xlat17.xyz = max(u_xlat17.xyz, float3(0.0, 0.0, 0.0));
    u_xlat17.xyz = min(u_xlat17.xyz, float3(255.0, 255.0, 255.0));
    u_xlati16.xyz = int3(u_xlat16.xzw);
    u_xlati16.xyz = u_xlati16.xyz * int3(0x1f, 0x3f, 0x1f) + int3(0x80, 0x80, 0x80);
    u_xlati18.xyz = u_xlati16.xyz >> int3(0x8, 0x8, 0x8);
    u_xlati16.xyz = u_xlati16.xyz + u_xlati18.xyz;
    u_xlati16.xyz = u_xlati16.xyz >> int3(0x8, 0x8, 0x8);
    u_xlati68 = u_xlati16.y << 0x5;
    u_xlati68 = u_xlati16.x * 0x800 + u_xlati68;
    u_xlat68 = as_type<float>(u_xlati68 + u_xlati16.z);
    TempArray6[0].x = u_xlat68;
    u_xlati16.xyz = int3(u_xlat17.xyz);
    u_xlati16.xyz = u_xlati16.xyz * int3(0x1f, 0x3f, 0x1f) + int3(0x80, 0x80, 0x80);
    u_xlati17.xyz = u_xlati16.xyz >> int3(0x8, 0x8, 0x8);
    u_xlati16.xyz = u_xlati16.xyz + u_xlati17.xyz;
    u_xlati16.xyz = u_xlati16.xyz >> int3(0x8, 0x8, 0x8);
    u_xlati69 = u_xlati16.y << 0x5;
    u_xlati69 = u_xlati16.x * 0x800 + u_xlati69;
    u_xlat69 = as_type<float>(u_xlati69 + u_xlati16.z);
    TempArray6[1].x = u_xlat69;
    u_xlatb70 = as_type<int>(u_xlat68)<as_type<int>(u_xlat69);
    if(u_xlatb70){
        TempArray6[0].x = u_xlat69;
        TempArray6[1].x = u_xlat68;
    }
    u_xlat68 = TempArray6[1].x;
    u_xlat69 = TempArray6[0].x;
    u_xlatb70 = as_type<int>(u_xlat69)<as_type<int>(u_xlat68);
    if(u_xlatb70){
        TempArray6[0].x = u_xlat68;
        TempArray6[1].x = u_xlat69;
    }
    u_xlat68 = TempArray6[1].x;
    u_xlat69 = TempArray6[0].x;
    u_xlati70 = as_type<int>(u_xlat68) * 0x10000 + as_type<int>(u_xlat69);
    TempArray7[0].xyz = u_xlat0.xyz;
    TempArray7[1].xyz = u_xlat1.xyz;
    TempArray7[2].xyz = u_xlat2.xyz;
    TempArray7[3].xyz = u_xlat3.xyz;
    TempArray7[4].xyz = u_xlat4.xyz;
    TempArray7[5].xyz = u_xlat5.xyz;
    TempArray7[6].xyz = u_xlat6.xyz;
    TempArray7[7].xyz = u_xlat7.xyz;
    TempArray7[8].xyz = u_xlat8.xyz;
    TempArray7[9].xyz = u_xlat9.xyz;
    TempArray7[10].xyz = u_xlat10.xyz;
    TempArray7[11].xyz = u_xlat11.xyz;
    TempArray7[12].xyz = u_xlat12.xyz;
    TempArray7[13].xyz = u_xlat13.xyz;
    TempArray7[14].xyz = u_xlat14.xyz;
    TempArray7[15].xyz = u_xlat15.xyz;
    u_xlatu0.xy = bitFieldExtractU(uint2(0x6u, 0x3u), uint2(0x5u, 0x2u), as_type<uint2>(float2(u_xlat69)));
    u_xlati44 = as_type<int>(u_xlat69) >> 0xb;
    u_xlati1.x = u_xlati44 >> 0x2;
    u_xlati44 = u_xlati44 * 0x8 + u_xlati1.x;
    u_xlat1.x = float(u_xlati44);
    u_xlati44 = int(u_xlatu0.x) >> 0x4;
    u_xlati0.x = int(u_xlatu0.x) * 0x4 + u_xlati44;
    u_xlat1.y = float(u_xlati0.x);
    u_xlati0.x = int(bitFieldInsert(0x5u, 0x3u, as_type<uint>(u_xlat69), 0x0u));
    u_xlati0.x = u_xlati0.x + int(u_xlatu0.y);
    u_xlat1.z = float(u_xlati0.x);
    u_xlatu0.xy = bitFieldExtractU(uint2(0x6u, 0x3u), uint2(0x5u, 0x2u), as_type<uint2>(float2(u_xlat68)));
    u_xlati44 = as_type<int>(u_xlat68) >> 0xb;
    u_xlati2.x = u_xlati44 >> 0x2;
    u_xlati44 = u_xlati44 * 0x8 + u_xlati2.x;
    u_xlat2.x = float(u_xlati44);
    u_xlati44 = int(u_xlatu0.x) >> 0x4;
    u_xlati0.x = int(u_xlatu0.x) * 0x4 + u_xlati44;
    u_xlat2.y = float(u_xlati0.x);
    u_xlati0.x = int(bitFieldInsert(0x5u, 0x3u, as_type<uint>(u_xlat68), 0x0u));
    u_xlati0.x = u_xlati0.x + int(u_xlatu0.y);
    u_xlat2.z = float(u_xlati0.x);
    u_xlat0.xyz = (-u_xlat1.xyz) + u_xlat2.xyz;
    u_xlat2.x = dot(u_xlat0.xyz, u_xlat0.xyz);
    u_xlat2.x = float(1.0) / float(u_xlat2.x);
    u_xlat2.x = u_xlat2.x * 3.0;
    u_xlat0.xyz = u_xlat0.xyz * u_xlat2.xxx;
    u_xlat2.x = float(0.5);
    u_xlati24 = int(0x0);
    while(true){
        u_xlatb46 = u_xlati24>=0x3;
        if(u_xlatb46){break;}
        u_xlat46 = dot(u_xlat1.xyz, ImmCB_0[u_xlati24].xyz);
        u_xlat68 = dot(u_xlat0.xyz, ImmCB_0[u_xlati24].xyz);
        u_xlat2.x = fma((-u_xlat46), u_xlat68, u_xlat2.x);
        u_xlati24 = u_xlati24 + 0x1;
    }
    u_xlati1.x = int(0x0);
    u_xlati23 = int(0x1);
    u_xlati45 = int(0x0);
    while(true){
        u_xlatb24 = u_xlati45>=0x10;
        if(u_xlatb24){break;}
        u_xlat24.xyz = TempArray7[u_xlati45].xyz;
        u_xlat24.x = dot(u_xlat24.xyz, u_xlat0.xyz);
        u_xlat24.x = u_xlat2.x + u_xlat24.x;
        u_xlati24 = int(u_xlat24.x);
        u_xlati24 = max(u_xlati24, 0x0);
        u_xlati24 = min(u_xlati24, 0x3);
        u_xlati1.x = u_xlati24 * u_xlati23 + u_xlati1.x;
        u_xlati23 = u_xlati23 << 0x2;
        u_xlati45 = u_xlati45 + 0x1;
    }
    u_xlatu0.x = uint(u_xlati1.x) & 0xaaaaaaaau;
    u_xlatu22 = u_xlatu0.x >> 0x1u;
    u_xlati44 = u_xlati1.x << 0x1;
    u_xlati44 = int(uint(u_xlati44) & 0xaaaaaaaau);
    u_xlati0.x = int(uint(u_xlati44) ^ u_xlatu0.x);
    u_xlati0.x = u_xlati0.x + int(u_xlatu22);
    u_xlati22 = int(mtl_ThreadID.y) * int(Globals.width_in_blocks) + int(mtl_ThreadID.x);
    u_xlati22 = u_xlati22 << 0x2;
    u_xlati22 = u_xlati22 + int(Globals.outputOffset);
    bufOutput[u_xlati22].value[(0x0 >> 2)] = uint(u_xlati66);
    u_xlati1.xyz = int3(u_xlati22) + int3(0x1, 0x2, 0x3);
    bufOutput[u_xlati1.x].value[(0x0 >> 2)] = uint(u_xlati67);
    bufOutput[u_xlati1.y].value[(0x0 >> 2)] = uint(u_xlati70);
    bufOutput[u_xlati1.z].value[(0x0 >> 2)] = uint(u_xlati0.x);
    return;
}
