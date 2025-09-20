#ifndef MATH_H
#define MATH_H
#include "pch.h"
//

class Math {
 public:
  template <typename T>
  static T Clamp(const T& x, const T& low, const T& high) {
    return x < low ? low : (x > high ? high : x);
  }

  static DirectX::XMFLOAT4X4 Identity4x4() {
    static DirectX::XMFLOAT4X4 I(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 1.0f);
    return I;
  }
};

#endif  // !MATH_H
