#include <librealsense2/rs.hpp>
#include <iostream>

int main() {
  try {
    rs2::context ctx;
    auto devices = ctx.query_devices();
    size_t device_count = devices.size();
    std::cout << "Found " << device_count << " device(s)" << std::endl;
    for (size_t i = 0; i < device_count; ++i) {
      auto dev = devices[i];
      std::cout << "  - " << dev.get_info(RS2_CAMERA_INFO_NAME)
                << " (" << dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER) << ")" << std::endl;
    }
    if (device_count == 0) {
      std::cerr << "No devices detected" << std::endl;
      return 1;
    }
    rs2::pipeline pipe;
    pipe.start();
    auto frames = pipe.wait_for_frames();
    auto depth = frames.get_depth_frame();
    std::cout << "Captured depth frame: " << depth.get_width() << "x" << depth.get_height()
              << " at frame #" << depth.get_frame_number() << std::endl;
    pipe.stop();
  } catch (const rs2::error& e) {
    std::cerr << "RealSense error: " << e.what() << std::endl;
    return 2;
  } catch (const std::exception& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return 3;
  }
  return 0;
}
