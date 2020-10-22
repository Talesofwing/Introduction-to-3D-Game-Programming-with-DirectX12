#define MaxLights 16

struct Light {
    float3 Strength;
    float FalloffStart; // point/spot light only
    float3 Direction;   // directional/spot light only
    float FalloffEnd;   // point/spot light only
    float3 Position;    // point light only
    float SpotPower;    // spot light only
};

struct Material {
    float4 DiffuseAlbedo;   // 漫反射率
    float3 FresnelR0;       // 材質菲涅耳值
    float Shininess;        // 光澤度 [0, 1]
};

// 線性衰減因子
float CalcAttenuation (float d, float falloffStart, float falloffEnd) {
    return saturate ((falloffEnd - d) / (falloffEnd - falloffStart));
}

// 石里克提出的一種逼近菲涅耳反射率的近似方法
// R0 = ( (n-1)/(n+1) )^2, n 是折射率
float3 SchlickFresnel (float3 R0, float3 normal, float3 lightVec) {
    float cosIncidentAngle = saturate (dot (normal, lightVec));

    float f0 = 1.0f - cosIncidentAngle;
    // 石里克近似法
    // Rf = R0 + (1 - R0) * (1 - cos (theta))^5
    // theta : 入射角
    float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

    return reflectPercent;
}

float3 BlinnPhong (float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat) {
    // m由光澤度推導而來, 而光澤度則根據粗糙度求得
    const float m = mat.Shininess * 256.0f;
    float3 halfVec = normalize (toEye + lightVec);

    float roughnessFactor = (m + 8.0f) * pow (max (dot (halfVec, normal), 0.0f), m) / 8.0f;

    // 利用石里克近似法,求出反射光的因子 (即反射光顏色[0~1])
    float3 fresnelFactor = SchlickFresnel (mat.FresnelR0, halfVec, lightVec);

    float3 specAlbedo = fresnelFactor * roughnessFactor;

    // 儘管我們進行的是LDR (low dynamic range, 低動態範圍)渲染,但spec (鏡面反射) 公式得到
    // 的結果仍會超出範圍[0, 1], 因此現將其按比例縮小一些
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

float3 ComputeDirectionalLight (Light L, Material mat, float3 normal, float3 toEye) {
    // 光向量與光線傳播的方向剛好相反
    float3 lightVec = -L.Direction;

    // 通過朗伯余弦定律按比例降低光強
    float ndotl = max (dot (lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;

    return BlinnPhong (lightStrength, lightVec, normal, toEye, mat);
}

float3 ComputePointLight (Light L, Material mat, float3 pos, float3 normal, float3 toEye) {
    // 光向量
    float3 lightVec = L.Position - pos;

    // 由表面到光源的距離
    float d = length (lightVec);

    // 範圍檢測
    if (d > L.FalloffEnd)
        return 0.0f;

    // 對光向量進行規范化處理
    lightVec /= d;

    // 通過朗伯余弦定律按比例降低光強
    float ndotl = max (dot (lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;

    // 根據距離計算光的衰減
    float att = CalcAttenuation (d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;

    return BlinnPhong (lightStrength, lightVec, normal, toEye, mat);
}

float3 ComputeSpotLight (Light L, Material mat, float3 pos, float3 normal, float3 toEye) {
    // 光向量
    float3 lightVec = L.Position - pos;

    // 由表面到光源的距離
    float d = length (lightVec);

    // 範圍檢測
    if (d > L.FalloffEnd)
        return 0.0f;

    // 對光向量進行規范化處理
    lightVec /= d;

    // 通過朗伯余弦定律按比例降低光強
    float ndotl = max (dot (lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;

    // 根據距離計算光的衰減
    float att = CalcAttenuation (d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;

    // 根據聚光灯照明模型對光強進行縮放處理
    float spotFactor = pow (max (dot (-lightVec, L.Direction), 0.0f), L.SpotPower);
    lightStrength *= spotFactor;

    return BlinnPhong (lightStrength, lightVec, normal, toEye, mat);
}

float4 ComputeLighting (Light gLights[MaxLights], Material mat,
                        float3 pos, float3 normal, float3 toEye,
                        float3 shadowFactor) {

    float3 result = 0.0f;

    int i = 0;

#if (NUM_DIR_LIGHTS > 0)
    for (i = 0; i < NUM_DIR_LIGHTS; ++i) {
        result += shadowFactor[i] * ComputeDirectionalLight (gLights[i], mat, normal, toEye);
    }
#endif

#if (NUM_POINT_LIGHTS > 0)
    for (i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; ++i) {
        result += ComputePointLight (gLights[i], mat, pos, normal, toEye);
    }
#endif

#if (NUM_SPOT_LIGHTS > 0)
    for (i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i) {
        result += ComputeSpotLight (gLights[i], mat, pos, normal, toEye);
    }
#endif 

    return float4(result, 0.0f);
}


