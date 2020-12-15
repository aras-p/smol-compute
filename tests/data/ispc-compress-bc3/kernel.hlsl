// BC3 compression code ported over from
// https://github.com/GameTechDev/ISPCTextureCompressor/blob/master/ispc_texcomp/kernel.ispc

// ugly, but makes BC1 compression 20% faster overall
void compute_covar_dc_ugly(out float covar[6], out float3 dc, float3 block[16])
{
    int k;
    float3 acc = 0;
    for (k = 0; k < 16; ++k)
        acc += block[k];
    dc = acc / 16;

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
}

int stb__Mul8Bit(int a, int b)
{
  int t = a*b + 128;
  return (t + (t >> 8)) >> 8;
}

uint stb__As16Bit(int r, int g, int b)
{
   return (stb__Mul8Bit(r,31) << 11) + (stb__Mul8Bit(g,63) << 5) + stb__Mul8Bit(b,31);
}

uint enc_rgb565(float3 c)
{
    return stb__As16Bit((int)c.r, (int)c.g, (int)c.b);
}

float3 dec_rgb565(int p)
{
    int c2 = (p>>0)&31;
    int c1 = (p>>5)&63;
    int c0 = (p>>11)&31;

    float3 c;
    c[0] = (c0<<3)+(c0>>2);
    c[1] = (c1<<2)+(c1>>4);
    c[2] = (c2<<3)+(c2>>2);
    return c;
}

void swap_ints(inout int u, inout int v)
{
    int t = u;
    u = v;
    v = t;
}

float3 ssymv(float covar[6], float3 b)
{
    float3 a;
    a[0] = covar[0]*b[0]+covar[1]*b[1]+covar[2]*b[2];
    a[1] = covar[1]*b[0]+covar[3]*b[1]+covar[4]*b[2];
    a[2] = covar[2]*b[0]+covar[4]*b[1]+covar[5]*b[2];
    return a;
}

void ssymv3(out float a[4], float covar[10], float b[4])
{
    a[0] = covar[0]*b[0]+covar[1]*b[1]+covar[2]*b[2];
    a[1] = covar[1]*b[0]+covar[4]*b[1]+covar[5]*b[2];
    a[2] = covar[2]*b[0]+covar[5]*b[1]+covar[7]*b[2];
    a[3] = 0;
}

void ssymv4(out float a[4], float covar[10], float b[4])
{
    a[0] = covar[0]*b[0]+covar[1]*b[1]+covar[2]*b[2]+covar[3]*b[3];
    a[1] = covar[1]*b[0]+covar[4]*b[1]+covar[5]*b[2]+covar[6]*b[3];
    a[2] = covar[2]*b[0]+covar[5]*b[1]+covar[7]*b[2]+covar[8]*b[3];
    a[3] = covar[3]*b[0]+covar[6]*b[1]+covar[8]*b[2]+covar[9]*b[3];
}

float3 compute_axis3(float covar[6], const uint powerIterations)
{
    float3 v = float3(1,1,1);
    for (uint i=0; i<powerIterations; i++)
    {
        v = ssymv(covar, v);
        if (i%2==1) // renormalize every other iteration
            v = normalize(v);
    }
    return v;
}

void compute_axis(out float axis[4], float covar[10], uint powerIterations, int channels)
{
    float vec[4] = {1,1,1,1};

    for (uint i=0; i<powerIterations; i++)
    {
        if (channels == 3) ssymv3(axis, covar, vec);
        if (channels == 4) ssymv4(axis, covar, vec);
        vec = axis;

        if (i%2==1) // renormalize every other iteration
        {
            int p;
            float norm_sq = 0;
            for (p=0; p<channels; p++)
                norm_sq += axis[p]*axis[p];

            float rnorm = 1.0 / sqrt(norm_sq);
            for (p=0; p<channels; p++) vec[p] *= rnorm;
        }
    }

    axis = vec;
}

void pick_endpoints(out float3 c0, out float3 c1, float3 block[16], float3 axis, float3 dc)
{
    float min_dot = 256*256;
    float max_dot = 0;

    for (int i = 0; i < 16; ++i)
    {
        float dt = dot(block[i]-dc, axis);
        min_dot = min(dt, min_dot);
        max_dot = max(dt, max_dot);
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

uint fast_quant(float3 block[16], int p0, int p1)
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

void bc1_refine(int pe[2], float3 block[16], uint bits, float3 dc)
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

uint fix_qbits(uint qbits)
{
    const uint mask_01b = 0x55555555;
    const uint mask_10b = 0xAAAAAAAA;

    uint qbits0 = qbits&mask_01b;
    uint qbits1 = qbits&mask_10b;
    qbits = (qbits1>>1) + (qbits1 ^ (qbits0<<1));

    return qbits;
}

uint2 CompressBlockBC1_core(float3 block[16])
{
    const int powerIterations = 4;
    const int refineIterations = 1;

    float covar[6];
    float3 dc;
    compute_covar_dc_ugly(covar, dc, block);

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
    [unroll]
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

uint2 CompressBlockBC3_alpha(float block[16])
{
    uint k;
    float ep[2] = {255, 0};

    for (k=0; k<16; k++)
    {
        ep[0] = min(ep[0], block[k]);
        ep[1] = max(ep[1], block[k]);
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

StructuredBuffer<uint> bufInput : register(t2);
RWStructuredBuffer<uint> bufOutput : register(u0);

cbuffer Globals : register(b1)
{
    uint inputOffset;
    uint outputOffset;
    uint image_width;
    uint image_height;
    uint width_in_blocks;
    uint height_in_blocks;
};

void LoadBlock(uint2 id, out float3 pixels[16], out float alphas[16])
{
    uint2 pixelCoordBase = id << 2;
    for (int i = 0; i < 16; ++i)
    {
        uint2 pixelCoord = pixelCoordBase + uint2(i & 3, i >> 2);
        pixelCoord = min(pixelCoord, uint2(image_width-1, image_height-1));
        uint raw = bufInput[pixelCoord.y*image_width+pixelCoord.x+inputOffset];
        float4 pix;
        pix.r = (float)(raw & 0xFF);
        pix.g = (float)((raw >> 8) & 0xFF);
        pix.b = (float)((raw >> 16) & 0xFF);
        pix.a = (float)(raw >> 24);
        pixels[i] = pix.rgb;
        alphas[i] = pix.a;
    }
}

[numthreads(8,8,1)]
void computeMain (uint3 id : SV_DispatchThreadID)
{
    if (id.x >= width_in_blocks || id.y >= height_in_blocks)
        return;

    float3 pixels[16];
    float alphas[16];
    LoadBlock(id.xy, pixels, alphas);

    uint2 resA = CompressBlockBC3_alpha(alphas);
    uint2 res = CompressBlockBC1_core(pixels);
    uint bufIdx = (id.y * width_in_blocks + id.x) * 4 + outputOffset;
    bufOutput[bufIdx + 0] = resA.x;
    bufOutput[bufIdx + 1] = resA.y;
    bufOutput[bufIdx + 2] = res.x;
    bufOutput[bufIdx + 3] = res.y;
}
