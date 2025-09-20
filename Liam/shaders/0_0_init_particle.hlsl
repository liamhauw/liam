#include "common.hlsli"

RWStructuredBuffer<Particle> gParticle : register(u5);

[numthreads(128, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
    uint index = dtid.x;
    uint seed = index + 22387;
    uint bomb_index = index % gBombCount;

    float4x4 bomb_model = gBombInfo[bomb_index].model;
    float4 bomb_mass_velocity = gBombInfo[bomb_index].mass_velocity;
    float bomb_length = gBombInfo[bomb_index].length;
    float bomb_radius = gBombInfo[bomb_index].radius;

    float r = bomb_radius * sqrt(UniformPRN0(0.01, 0.99, seed));
    float theta = UniformPRN1(0.01, 6.283185308, seed);
    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = UniformPRN2(-0.499 * bomb_length, 0.499 * bomb_length, seed);

    float3 position = mul(float4(x, y, z, 1.0), bomb_model).xyz;
    // 初始化粒子位置
    gParticle[index].gPosition = position;

    float2 n0 = NormalPRN0(bomb_mass_velocity.x, bomb_mass_velocity.y, seed);
    // 初始化粒子质量
    gParticle[index].gMass = max(n0.x, 0.0);

    float3 p0 = mul(float4(0.0, 0.0, -0.499 * bomb_length, 1.0), bomb_model).xyz;
    float3 p1 = mul(float4(0.0, 0.0, 0.499 * bomb_length, 1.0), bomb_model).xyz;


    float3 a = position - p0;
    float3 b = p1 - p0;
    float3 c = dot(a, b) / pow(length(b), 2) * b;
    float3 d = a - c;
    float2 n1 = NormalPRN0(bomb_mass_velocity.z, bomb_mass_velocity.w, seed);
    float3 vel = max(0.0, n1.y) * normalize(d);

    // 初始化粒子速度
    gParticle[index].gVelocity = vel;
}