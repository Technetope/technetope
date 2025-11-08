#pragma once

#include <array>
#include <string>

namespace locomotion::calibration {

struct CameraIntrinsics {
  double fx{0.0};
  double fy{0.0};
  double cx{0.0};
  double cy{0.0};
  std::string distortion_model{"brown_conrady"};
  std::array<double, 5> distortion_coeffs{};
};

}  // namespace locomotion::calibration
