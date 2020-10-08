cbuffer cbPerObject : register (b0) {
	float4x4 gWorld;
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