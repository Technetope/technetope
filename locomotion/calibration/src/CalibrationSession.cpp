#include "locomotion/calibration/CalibrationSession.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <ctime>

#include <nlohmann/json.hpp>

namespace locomotion::calibration {

namespace {

std::string formatTimestamp(const std::chrono::system_clock::time_point& tp) {
  std::time_t tt = std::chrono::system_clock::to_time_t(tp);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &tt);
#else
  gmtime_r(&tt, &tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

}  // namespace

CalibrationSession::CalibrationSession(CalibrationPipeline pipeline,
                                       SessionConfig session_config)
    : pipeline_(std::move(pipeline)), session_config_(std::move(session_config)) {
  if (session_config_.attempts <= 0) {
    session_config_.attempts = pipeline_.config().session_attempts;
  }
  if (session_config_.max_plane_std_mm <= 0.0) {
    session_config_.max_plane_std_mm = pipeline_.config().max_plane_std_mm;
  }
  if (session_config_.min_inlier_ratio <= 0.0) {
    session_config_.min_inlier_ratio = pipeline_.config().floor_min_inlier_ratio;
  }
}

std::optional<CalibrationResult> CalibrationSession::Run() {
  if (!pipeline_.initialize()) {
    spdlog::error("Failed to initialize CalibrationPipeline.");
    return std::nullopt;
  }

  std::optional<CalibrationResult> best;
  int successes = 0;

  for (int attempt = 0; attempt < session_config_.attempts; ++attempt) {
    auto snapshot = pipeline_.runOnce();
    if (!snapshot) {
      spdlog::info("Attempt {}: ChArUco detection failed.", attempt + 1);
      continue;
    }

    if (snapshot->reprojection_error > pipeline_.config().max_reprojection_error_id) {
      spdlog::warn("Attempt {}: reprojection error {:.3f} exceeds threshold {:.3f}.",
                   attempt + 1, snapshot->reprojection_error,
                   pipeline_.config().max_reprojection_error_id);
      continue;
    }

    if (pipeline_.config().enable_floor_plane_fit &&
        snapshot->floor_plane_std_mm > session_config_.max_plane_std_mm) {
      spdlog::warn("Attempt {}: plane std {:.3f} exceeds threshold {:.3f}.", attempt + 1,
                   snapshot->floor_plane_std_mm, session_config_.max_plane_std_mm);
      continue;
    }

    if (pipeline_.config().enable_floor_plane_fit &&
        snapshot->inlier_ratio < session_config_.min_inlier_ratio) {
      spdlog::warn("Attempt {}: inlier ratio {:.3f} below minimum {:.3f}.", attempt + 1,
                   snapshot->inlier_ratio, session_config_.min_inlier_ratio);
      continue;
    }

    CalibrationResult result = ToResult(*snapshot);
    if (!best || result.timestamp > best->timestamp) {
      best = result;
    }
    ++successes;
  }

  if (!best) {
    spdlog::error("CalibrationSession failed. No valid snapshots collected out of {} attempts.",
                  session_config_.attempts);
    return std::nullopt;
  }

  spdlog::info("CalibrationSession succeeded with {} valid snapshots.", successes);
  return best;
}

CalibrationResult CalibrationSession::ToResult(const CalibrationSnapshot& snapshot) {
  CalibrationResult result;
  result.intrinsics = snapshot.intrinsics;
  result.homography = snapshot.homography_color_to_position.clone();
  result.floor_plane = snapshot.floor_plane;
  result.reprojection_error = snapshot.reprojection_error;
  result.floor_plane_std_mm = snapshot.floor_plane_std_mm;
  result.inlier_ratio = snapshot.inlier_ratio;
  result.detected_charuco_corners = snapshot.detected_charuco_corners;
  result.timestamp = snapshot.timestamp;
  return result;
}

bool CalibrationSession::SaveResultJson(const CalibrationResult& result,
                                        const std::string& path) const {
  nlohmann::json j;
  j["schema_version"] = "2.0";
  j["timestamp"] = formatTimestamp(result.timestamp);
  j["reprojection_error_id"] = result.reprojection_error;

  nlohmann::json intrinsics_json;
  intrinsics_json["fx"] = result.intrinsics.fx;
  intrinsics_json["fy"] = result.intrinsics.fy;
  intrinsics_json["cx"] = result.intrinsics.cx;
  intrinsics_json["cy"] = result.intrinsics.cy;
  intrinsics_json["distortion_model"] = result.intrinsics.distortion_model;
  intrinsics_json["distortion_coeffs"] = result.intrinsics.distortion_coeffs;
  j["intrinsics"] = intrinsics_json;

  nlohmann::json floor_plane_json;
  floor_plane_json["coefficients"] = {result.floor_plane[0], result.floor_plane[1],
                                      result.floor_plane[2], result.floor_plane[3]};
  floor_plane_json["std_mm"] = result.floor_plane_std_mm;
  floor_plane_json["inlier_ratio"] = result.inlier_ratio;
  j["floor_plane"] = floor_plane_json;
  j["charuco_corners"] = result.detected_charuco_corners;

  auto& h = j["homography_color_to_position"];
  h = nlohmann::json::array();
  for (int r = 0; r < result.homography.rows; ++r) {
    nlohmann::json row = nlohmann::json::array();
    for (int c = 0; c < result.homography.cols; ++c) {
      row.push_back(result.homography.at<double>(r, c));
    }
    h.push_back(row);
  }

  const auto& pipeline_cfg = pipeline_.config();
  bool repro_pass = result.reprojection_error <= pipeline_cfg.max_reprojection_error_id;
  bool plane_std_pass = true;
  bool inlier_pass = true;

  if (pipeline_cfg.enable_floor_plane_fit) {
    plane_std_pass = result.floor_plane_std_mm <= session_config_.max_plane_std_mm;
    inlier_pass = result.inlier_ratio >= session_config_.min_inlier_ratio;
  }

  nlohmann::json checks;
  checks["reprojection_error"] = repro_pass ? "PASS" : "FAIL";
  checks["floor_plane_std"] =
      pipeline_cfg.enable_floor_plane_fit ? (plane_std_pass ? "PASS" : "FAIL") : "SKIP";
  checks["floor_inlier_ratio"] =
      pipeline_cfg.enable_floor_plane_fit ? (inlier_pass ? "PASS" : "FAIL") : "SKIP";

  j["validation"] = {
      {"passed", repro_pass && plane_std_pass && inlier_pass},
      {"checks", checks}};

  try {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream ofs(path);
    ofs << j.dump(2);
    spdlog::info("Calibration result saved to {}", path);
    return true;
  } catch (const std::exception& ex) {
    spdlog::error("Failed to save calibration result to {}: {}", path, ex.what());
    return false;
  }
}

}  // namespace locomotion::calibration
