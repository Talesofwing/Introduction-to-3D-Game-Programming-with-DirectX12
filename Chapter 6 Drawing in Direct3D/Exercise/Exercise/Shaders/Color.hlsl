cbuffer cbPerObject : register(b0) {
	float4x4 gWorldViewProj;

	// Exercise 16
	float4 gPulseColor;

	// Exercise 6
	float gTime;
}

struct VS_IN {
	float3 pos : POSITION;
	float4 tangent : TANGENT;

	// Exercise 1
	//float3 normal : NORMAL;
	//float2 tex0 : TEXCOORD0;
	//float2 tex1 : TEXCOORD1;
	//float4 color : COLOR;
};

struct VS_OUT {
	float4 pos : SV_POSITION;
	float4 color : COLOR;
};

VS_OUT VS(VS_IN input) {
	VS_OUT output;

	// Exercise 6
	input.pos.xy += 0.5f * sin(input.pos.x) * sin(3.0f * gTime);
	input.pos.z *= 0.6f + 0.4f * sin(2.0f * gTime);

	// Transform to homogeneous clip space.
	output.pos = mul(gWorldViewProj, float4(input.pos, 1.0f));
	// Just pass vertex color into the pixel shader.
	output.color = input.tangent;

	return output;
}

float4 PS(VS_OUT input) : SV_Target {
	// Exercise 16
	const float pi = 3.14159;
	float s = 0.5f * sin(2 * gTime - 0.25f * pi) + 0.5f;
	float4 c = lerp(input.color, gPulseColor, s);
	return c;

	// Exercise 15
	// clip(x) : Limitedto use in Pixel Shader, if x < 0, discard the current pixel
	clip(input.color.r - 0.5f);
	return input.color;

	// Exercise 14
	return input.color * sin(gTime);

	return input.color;
}
