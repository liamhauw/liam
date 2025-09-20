#ifndef CAMERA_H
#define CAMERA_H
#include "pch.h"
//

#include "parameter.h"

struct Camera {
  float theta = 1.3f * DirectX::XM_PI;
  float phi = DirectX::XM_PIDIV2 * 0.7;
  float radius = kViewDistance;
};

#endif  // !CAMERA_H
