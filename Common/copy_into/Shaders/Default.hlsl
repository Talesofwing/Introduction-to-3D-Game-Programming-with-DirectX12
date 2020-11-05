#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

#include "LightingUtil.hlsl"

Texture2D    gDiffuseMap : register(t0);

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbPerObject : register(b0) {
    float4x4 gWorld;
    float4x4 gTexTransform;
};

cbuffer cbMaterial : register(b2) {
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float  gRoughness;
    float4x4 gMatTransform;
};

cbuffer cbPass : register(b1) {
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;

    float4 gAmbientLight;

    // 對於每個以MaxLights為光源數量最大值的對象來說
    // 索引[0, NUM_DIR_LIGHTS)表示的是方向光源
    // 索引[NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS)表示的是點光源
    // 索引[NUM_DIR_LIGHTS + NUM_POINT_LIGHTS, NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS)則表示的是聚光灯光源
    Light gLights[MaxLights];
};

struct VertexIn {
    float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut {
    float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

VertexOut VS (VertexIn vin) {
    VertexOut vout = (VertexOut)0.0f;

    // 將頂點變換到世界空間
    float4 posW = mul (float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    // 假設這裏進行的是等比縮放,否則這裏需要使用世界矩陣的逆轉置矩陣
    vout.NormalW = mul (vin.NormalL, (float3x3)gWorld);

    // 將頂點變換到齊次截剪空間
    vout.PosH = mul (posW, gViewProj);

    float4 texC = mul (float4 (vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul (texC, gMatTransform).xy;

    return vout;
}

float4 PS (VertexOut pin) : SV_Target {
    float4 diffuseAlbedo = gDiffuseMap.Sample (gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;

    // 對法線插值可能導致其非規範化,因此需要再次對他進行規範化處理
    pin.NormalW = normalize (pin.NormalW);

    // 光線經表面上一點反射到觀察點這一方向上的向量
    float3 toEyeW = gEyePosW - pin.PosW;
    float distToEye = length (toEyeW);
    toEyeW /= distToEye;

    // 間接光照
    float4 ambient = gAmbientLight * diffuseAlbedo;

    // 直接光照
    const float shininess = 1.0f - gRoughness;
    Material mat = {diffuseAlbedo, gFresnelR0, shininess};
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting (gLights, mat, pin.PosW,
                                          pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

    // 從漫反射材質中獲取alpha值的常見手段
    litColor.a = diffuseAlbedo.a;

    return litColor;
}