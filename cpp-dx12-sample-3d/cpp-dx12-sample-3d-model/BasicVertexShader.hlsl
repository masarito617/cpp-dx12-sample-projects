#include"BasicType.hlsli"

cbuffer cbuff0 : register(b0) {
	matrix mat; // �ϊ��s��
}

BasicType BasicVS(float4 pos : POSITION, float2 uv : TEXCOORD)
{
	BasicType output;
	output.position = mul(mat, pos); // HLSL�ł͗�D��
	output.uv = uv;
	return output;
}