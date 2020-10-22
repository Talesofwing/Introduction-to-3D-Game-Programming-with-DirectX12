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


cbuffer cbPerObject : register(b0) {
    float4x4 gWorld;
};

cbuffer cbMaterial : register(b1) {
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float  gRoughness;
    float4x4 gMatTransform;
};

cbuffer cbPass : register(b2) {
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

    // ���ÿ����MaxLights���Դ�������ֵ�Č�����f
    // ����[0, NUM_DIR_LIGHTS)��ʾ���Ƿ����Դ
    // ����[NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS)��ʾ�����c��Դ
    // ����[NUM_DIR_LIGHTS + NUM_POINT_LIGHTS, NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS)�t��ʾ���Ǿ۹�ƹ�Դ
    Light gLights[MaxLights];
};
 
struct VertexIn {
    float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
};

struct VertexOut {
    float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
};

VertexOut VS (VertexIn vin) {
    VertexOut vout = (VertexOut)0.0f;

    // ����c׃�Q��������g
    float4 posW = mul (float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    // ���O�@�Y�M�е��ǵȱȿs��,��t�@�Y��Ҫʹ�������ꇵ����D�þ��
    vout.NormalW = mul (vin.NormalL, (float3x3)gWorld);

    // ����c׃�Q���R�νؼ����g
    vout.PosH = mul (posW, gViewProj);

    return vout;
}

float4 PS (VertexOut pin) : SV_Target {
    // ��������ֵ���܌������Ҏ����,�����Ҫ�ٴΌ����M��Ҏ����̎��
    pin.NormalW = normalize (pin.NormalW);

    // �⾀��������һ�c���䵽�^���c�@һ�����ϵ�����
    float3 toEyeW = normalize (gEyePosW - pin.PosW);

    // �g�ӹ���
    float4 ambient = gAmbientLight * gDiffuseAlbedo;

    // ֱ�ӹ���
    const float shininess = 1.0f - gRoughness;
    Material mat = {gDiffuseAlbedo, gFresnelR0, shininess};
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting (gLights, mat, pin.PosW,
                                          pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

    // ����������|�Ы@ȡalphaֵ�ĳ�Ҋ�ֶ�
    litColor.a = gDiffuseAlbedo.a;

    return litColor;
}


