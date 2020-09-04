cbuffer cbPerObject : register (b0) {
	float4x4 gWorldViewProj;
}

struct VS_IN {
	float3 pos : POSITION;
	float4 color : COLOR0;
};

struct VS_OUT {
	float4 pos : SV_POSITION;
	float4 color : COLOR;
};

VS_OUT VS(VS_IN input) {
	VS_OUT output;

	// Transform to homogeneous clip space.
	output.pos = mul (gWorldViewProj, float4 (input.pos, 1.0f));
	// Just pass vertex color into the pixel shader.
	output.color = input.color;

	return output;
}

float4 PS (VS_OUT input) : SV_Target {
	return input.color;
}