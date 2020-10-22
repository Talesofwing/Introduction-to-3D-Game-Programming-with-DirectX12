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
    float4 DiffuseAlbedo;   // ��������
    float3 FresnelR0;       // ���|������ֵ
    float Shininess;        // ��ɶ� [0, 1]
};

// ����˥�p����
float CalcAttenuation (float d, float falloffStart, float falloffEnd) {
    return saturate ((falloffEnd - d) / (falloffEnd - falloffStart));
}

// ʯ��������һ�N�ƽ������������ʵĽ��Ʒ���
// R0 = ( (n-1)/(n+1) )^2, n ��������
float3 SchlickFresnel (float3 R0, float3 normal, float3 lightVec) {
    float cosIncidentAngle = saturate (dot (normal, lightVec));

    float f0 = 1.0f - cosIncidentAngle;
    // ʯ��˽��Ʒ�
    // Rf = R0 + (1 - R0) * (1 - cos (theta))^5
    // theta : �����
    float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

    return reflectPercent;
}

float3 BlinnPhong (float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat) {
    // m�ɹ�ɶ��ƌ�����, ����ɶȄt�����ֲڶ����
    const float m = mat.Shininess * 256.0f;
    float3 halfVec = normalize (toEye + lightVec);

    float roughnessFactor = (m + 8.0f) * pow (max (dot (halfVec, normal), 0.0f), m) / 8.0f;

    // ����ʯ��˽��Ʒ�,������������� (��������ɫ[0~1])
    float3 fresnelFactor = SchlickFresnel (mat.FresnelR0, halfVec, lightVec);

    float3 specAlbedo = fresnelFactor * roughnessFactor;

    // �����҂��M�е���LDR (low dynamic range, �̈́ӑB����)��Ⱦ,��spec (�R�淴��) ��ʽ�õ�
    // �ĽY���ԕ���������[0, 1], ��ˬF���䰴�����sСһЩ
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

float3 ComputeDirectionalLight (Light L, Material mat, float3 normal, float3 toEye) {
    // �������c�⾀�����ķ��򄂺��෴
    float3 lightVec = -L.Direction;

    // ͨ�^�ʲ����Ҷ��ɰ��������͹⏊
    float ndotl = max (dot (lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;

    return BlinnPhong (lightStrength, lightVec, normal, toEye, mat);
}

float3 ComputePointLight (Light L, Material mat, float3 pos, float3 normal, float3 toEye) {
    // ������
    float3 lightVec = L.Position - pos;

    // �ɱ��浽��Դ�ľ��x
    float d = length (lightVec);

    // �����z�y
    if (d > L.FalloffEnd)
        return 0.0f;

    // ���������M��Ҏ����̎��
    lightVec /= d;

    // ͨ�^�ʲ����Ҷ��ɰ��������͹⏊
    float ndotl = max (dot (lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;

    // �������xӋ����˥�p
    float att = CalcAttenuation (d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;

    return BlinnPhong (lightStrength, lightVec, normal, toEye, mat);
}

float3 ComputeSpotLight (Light L, Material mat, float3 pos, float3 normal, float3 toEye) {
    // ������
    float3 lightVec = L.Position - pos;

    // �ɱ��浽��Դ�ľ��x
    float d = length (lightVec);

    // �����z�y
    if (d > L.FalloffEnd)
        return 0.0f;

    // ���������M��Ҏ����̎��
    lightVec /= d;

    // ͨ�^�ʲ����Ҷ��ɰ��������͹⏊
    float ndotl = max (dot (lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;

    // �������xӋ����˥�p
    float att = CalcAttenuation (d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;

    // �����۹������ģ�͌��⏊�M�пs��̎��
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


