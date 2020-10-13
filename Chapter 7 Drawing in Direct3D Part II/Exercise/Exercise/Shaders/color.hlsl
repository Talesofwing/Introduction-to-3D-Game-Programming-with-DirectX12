cbuffer cbPerObject : register (b0) {
	float4x4 gWorld;

	//float4 w0;
	//float4 w1;
	//float4 w2;
	//float4 w3;

	//float w1;
	//float w2;
	//float w3;
	//float w4;
	//float w5;
	//float w6;
	//float w7;
	//float w8;
	//float w9;
	//float w10;
	//float w11;
	//float w12;
	//float w13;
	//float w14;
	//float w15;
	//float w16;
};

cbuffer cbPass : register (b1) {
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
};

struct VertexIn {
	float3 PosL : POSITION;
	float4 Color : COLOR;
	float3 Normal : NORMAL;
};

struct VertexOut {
	float4 PosH : SV_POSITION;
	float4 Color : COLOR;
};

// PosL : position of local
// PosW : position of world
// PosH : position of homogeneous

VertexOut VS (VertexIn vin) {
	VertexOut vout;

	//float4x4 gWorld = {1, 0, 0, 0,
	//	0, 1, 0, 0,
	//	0, 0, 1, 0,
	//	0, 0, 0, 1};

	//float4x4 gWorld = {w1, w2, w3, w4,
	//	w5, w6, w7, w8,
	//	w9, w10, w11, w12,
	//	w13, w14, w15, w16
	//};

	//float4x4 gWorld = (w0.x, w0.y, w0.z, w0.w,
	//	w1.x, w1.y, w1.z, w1.w,
	//	w2.x, w2.y, w2.z, w2.w,
	//	w3.x, w3.y, w3.z, w3.w);

	// Transform to homogeneous clip space.
	// mul 為了滿足某些東西, 會對矩陣再次轉置
	float4 posW = mul (float4 (vin.PosL, 1.0f), gWorld);
	vout.PosH = mul (posW, gViewProj);

	// Just pass vertex color into the pixel shader.
	vout.Color = vin.Color;

	return vout;
}

float4 PS (VertexOut pin) : SV_Target {
	return pin.Color;
}