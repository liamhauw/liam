#define MAX_BOMB_COUNT 6

struct BombInfo {
    float4x4 model;
    float4 mass_velocity;
    float length;
    float radius;
    float pad0;
    float pad1;
};

struct Particle {
  float3 gPosition;
  float gMass;
  float3 gVelocity;
};

// const buffer
cbuffer SolverCB : register(b0) {
  BombInfo gBombInfo[MAX_BOMB_COUNT];

  uint gBombCount;
  uint3 gCellCount;

  float gCellSize;
  float3 gGridSize;

  float gAmbientTemp;
  float3 gGridLbnPos;

  float3 gGridRtfPos;
  float gAlpha;

  float gBeta;
  float gEpsilon;
  float gAtteMass;
  float gAtteVel;

  float3 gWindPower;
  float gEmDens;

  float gEmtemp;
  float gDt;
};

cbuffer SceneCB : register(b1) {
  float4x4 gMvp;
  float4x4 gInvModel;
  float4 gViewPos;

  float3 gLightPos;
  float gLightRadiance;

  uint gWaveBand;
  float3 gExt;
};


float f0(float dens) {
  dens /= 1000.0;
  return  8e-05 * pow(dens, 4) - 0.0001 * pow(dens, 3) + 7E-05 * pow(dens, 2) + 0.3118 * dens + 1e-07;
}

float Lbb(float temp) {
  if (gWaveBand == 0) {
    return 0.0;
  } else if (gWaveBand == 1) {
    return max(0.0, -1.749e-12 * pow(temp, 4) + 6.072e-9 * pow(temp, 3) + -5.255e-6 * pow(temp, 2) + 0.001819 * temp - 0.226); 
  } else {
    return max(0.0, 9.535e-14 * pow(temp, 4) - 4.043e-10 * pow(temp, 3) + 7.054e-7 * pow(temp, 2) - 0.0002667 * temp + 0.03055);
  }
}

// generate a uniformly distributed pseudo-random number (min_value - max_value) 
float UniformPRN0(float min_value, float max_value, uint seed) {
  float a = frac(sin(seed * 78.233) * 43758.5453);
  float res = a * (max_value - min_value) + min_value;
  return res;
}

float UniformPRN1(float min_value, float max_value, uint seed) {
  seed += ( seed << 10u );
  seed ^= ( seed >>  6u );
  seed += ( seed <<  3u );
  seed ^= ( seed >> 11u );
  seed += ( seed << 15u );
  float a = float(seed) / 4294967295.0;
  float res = a * (max_value - min_value) + min_value;
  return res;
}


float UniformPRN2(float min_value, float max_value, uint seed)
{
  seed ^= 2747636419;
  seed *= 2654435769;
  seed ^= seed >> 16;
  seed *= 2654435769;
  seed ^= seed >> 16;
  seed *= 2654435769;
  float a = float(seed) / 4294967295.0;
  float res = a * (max_value - min_value) + min_value;
  return res;
}

float UniformPRN3(float min_value, float max_value, uint seed) {
  float a = frac(sin(seed * 68.492) * 769213.8621);
  float res = a * (max_value - min_value) + min_value;
  return res;
}

float UniformPRN0(float min_value, float max_value, uint3 seed) {
  float a = frac(sin(dot(seed, float3(6.2383, 12.9898, 78.233))) * 43758.5453);
  float res = a * (max_value - min_value) + min_value;
  return res;
}

// generate pairs of independent normally distributed pseudo-random numbers
float2 NormalPRN0(float mu, float sigma, uint seed) {
  float un0 = UniformPRN0(0.01, 0.99, seed);
  float un1 = UniformPRN1(0.01, 0.99, seed);
  float two_pi = 6.2831855;
  float mag = sigma * sqrt(-2.0 * log(un0));
  float nn0 = mag * cos(two_pi * un1) + mu; 
  float nn1 = mag * sin(two_pi * un1) + mu; 
  return float2(nn0, nn1);
}

float2 NormalPRN1(float mu, float sigma, uint seed) {
  float un0 = UniformPRN2(0.01, 0.99, seed);
  float un1 = UniformPRN3(0.01, 0.99, seed);
  float two_pi = 6.2831855;
  float mag = sigma * sqrt(-2.0 * log(un0));
  float nn0 = mag * cos(two_pi * un1) + mu; 
  float nn1 = mag * sin(two_pi * un1) + mu; 
  return float2(nn0, nn1);
}

// dtid to texture position
float3 UDtid2Tpos(uint3 dtid) {
  return (dtid + float3(0.5, 0.5, 0.5)) / (gCellCount + float3(1.0, 0.0, 0.0));
}

float3 VDtid2Tpos(uint3 dtid) {
  return (dtid + float3(0.5, 0.5, 0.5)) / (gCellCount + float3(0.0, 1.0, 0.0));
}

float3 WDtid2Tpos(uint3 dtid) {
  return (dtid + float3(0.5, 0.5, 0.5)) / (gCellCount + float3(0.0, 0.0, 1.0));
}

float3 VoxelCenterDtid2Tpos(uint3 dtid) {
  return (dtid + float3(0.5, 0.5, 0.5)) / gCellCount;
}

// dtid to model position
float3 UDtid2Mpos(uint3 dtid) {
  return gGridLbnPos + (dtid + float3(0.0, 0.5, 0.5)) * gCellSize;
}

float3 VDtid2Mpos(uint3 dtid) {
  return gGridLbnPos + (dtid + float3(0.5, 0.0, 0.5)) * gCellSize;
}

float3 WDtid2Mpos(uint3 dtid) {
  return gGridLbnPos + (dtid + float3(0.5, 0.5, 0.0)) * gCellSize;
}

float3 VoxelCenterDtid2Mpos(uint3 dtid) {
  return gGridLbnPos + (dtid + float3(0.5, 0.5, 0.5)) * gCellSize;
}

// grid model position to texture position
float3 UMpos2Tpos(float3 mpos) {
  return (mpos - (gGridLbnPos - float3(0.5 * gCellSize, 0.0, 0.0))) 
       / (gGridSize + float3(gCellSize, 0.0 ,0.0));
}

float3 VMpos2Tpos(float3 mpos) {
  return (mpos - (gGridLbnPos - float3(0.0, 0.5 * gCellSize, 0.0)))
       / (gGridSize + float3(0.0, gCellSize, 0.0));
}

float3 WMpos2Tpos(float3 mpos) {
  return (mpos - (gGridLbnPos - float3(0.0, 0.0, 0.5 * gCellSize)))
       / (gGridSize + float3(0.0, 0.0, gCellSize));
}

float3 VoxelCenterMpos2Tpos(float3 mpos) {
  return (mpos - gGridLbnPos) / gGridSize;
}