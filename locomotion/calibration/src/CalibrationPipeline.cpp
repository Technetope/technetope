#include "locomotion/calibration/CalibrationPipeline.h"

#include <spdlog/spdlog.h>

#include <cmath>
#include <limits>
#include <map>
#include <utility>

#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>

namespace locomotion::calibration {

namespace {

rs2::config buildRealSenseConfig(const CalibrationConfig& cfg) {
  rs2::config rs_cfg;
  rs_cfg.enable_stream(RS2_STREAM_COLOR, cfg.color_width, cfg.color_height,
                       RS2_FORMAT_BGR8, cfg.fps);
  rs_cfg.enable_stream(RS2_STREAM_DEPTH, cfg.depth_width, cfg.depth_height,
                       RS2_FORMAT_Z16, cfg.fps);
  return rs_cfg;
}

cv::aruco::PredefinedDictionaryType parseDictionary(const std::string& name) {
  using PD = cv::aruco::PredefinedDictionaryType;
  static const std::map<std::string, PD> kMap = {
      {"DICT_4X4_50", PD::DICT_4X4_50},
      {"DICT_4X4_100", PD::DICT_4X4_100},
      {"DICT_5X5_50", PD::DICT_5X5_50},
      {"DICT_5X5_100", PD::DICT_5X5_100},
      {"DICT_6X6_50", PD::DICT_6X6_50},
      {"DICT_6X6_100", PD::DICT_6X6_100},
      {"DICT_APRILTAG_16h5", PD::DICT_APRILTAG_16h5},
      {"DICT_APRILTAG_25h9", PD::DICT_APRILTAG_25h9},
      {"DICT_APRILTAG_36h11", PD::DICT_APRILTAG_36h11}};
  auto it = kMap.find(name);
  if (it == kMap.end()) {
    spdlog::warn("Unknown ArUco dictionary '{}', fallback to DICT_4X4_50", name);
    return PD::DICT_4X4_50;
  }
  return it->second;
}

cv::Ptr<cv::aruco::Dictionary> makeDictionary(const CalibrationConfig& config) {
  auto dict_id = parseDictionary(config.aruco_dictionary);
  return cv::makePtr<cv::aruco::Dictionary>(cv::aruco::getPredefinedDictionary(dict_id));
}

spdlog::level::level_enum parseLogLevel(const std::string& name) {
  static const std::map<std::string, spdlog::level::level_enum> kLevels = {
      {"trace", spdlog::level::trace}, {"debug", spdlog::level::debug},
      {"info", spdlog::level::info},   {"warn", spdlog::level::warn},
      {"error", spdlog::level::err},   {"critical", spdlog::level::critical}};
  auto it = kLevels.find(name);
  if (it == kLevels.end()) {
    spdlog::warn("Unknown log level '{}', fallback to 'info'", name);
    return spdlog::level::info;
  }
  return it->second;
}

std::string distortionModelToString(rs2_distortion model) {
  switch (model) {
    case RS2_DISTORTION_NONE:
      return "none";
    case RS2_DISTORTION_MODIFIED_BROWN_CONRADY:
      return "modified_brown_conrady";
    case RS2_DISTORTION_INVERSE_BROWN_CONRADY:
      return "inverse_brown_conrady";
    case RS2_DISTORTION_FTHETA:
      return "f_theta";
    case RS2_DISTORTION_BROWN_CONRADY:
      return "brown_conrady";
    case RS2_DISTORTION_KANNALA_BRANDT4:
      return "kannala_brandt4";
    default:
      return "unknown";
  }
}

}  // namespace

CalibrationPipeline::CalibrationPipeline(CalibrationConfig config)
    : config_(std::move(config)) {}

CalibrationPipeline::~CalibrationPipeline() {
  try {
    pipeline_.stop();
  } catch (const rs2::error& err) {
    spdlog::warn("RealSense pipeline stop failed: {}", err.what());
  }
}

bool CalibrationPipeline::initialize() {
  spdlog::set_level(parseLogLevel(config_.log_level));
  dictionary_ = makeDictionary(config_);
  board_ = cv::makePtr<cv::aruco::CharucoBoard>(
      cv::Size(config_.charuco_squares_x, config_.charuco_squares_y),
      config_.charuco_square_length_mm, config_.charuco_marker_length_mm, *dictionary_);

  charuco_config_.min_corners = config_.min_charuco_corners;
  charuco_config_.enable_subpixel_refine = config_.charuco_enable_subpixel_refine;
  charuco_config_.subpixel_window = cv::Size(config_.charuco_subpixel_window,
                                             config_.charuco_subpixel_window);
  charuco_config_.subpixel_max_iterations = config_.charuco_subpixel_max_iterations;
  charuco_config_.subpixel_epsilon = config_.charuco_subpixel_epsilon;
  charuco_detector_ =
      std::make_unique<CharucoDetector>(dictionary_, board_, charuco_config_);

  if (config_.enable_floor_plane_fit) {
    FloorPlaneEstimatorConfig floor_config;
    floor_config.inlier_threshold_mm = config_.floor_inlier_threshold_mm;
    floor_config.ransac_iterations = config_.floor_ransac_iterations;
    floor_config.min_inlier_ratio = config_.floor_min_inlier_ratio;
    floor_config.z_min_mm = config_.floor_z_min_mm;
    floor_config.z_max_mm = config_.floor_z_max_mm;
    floor_config.downsample_grid = config_.floor_downsample_grid;
    floor_config.random_seed = config_.random_seed;
    floor_estimator_ = std::make_unique<FloorPlaneEstimator>(floor_config);
  } else {
    floor_estimator_.reset();
  }

  try {
    playmat_layout_ = PlaymatLayout::LoadFromFile(config_.playmat_layout_path);
    has_playmat_layout_ = true;
    spdlog::info("Loaded playmat layout from '{}'", config_.playmat_layout_path);
  } catch (const std::exception& ex) {
    has_playmat_layout_ = false;
    spdlog::warn("Failed to load playmat layout '{}': {}", config_.playmat_layout_path, ex.what());
  }

  try {
    profile_ = pipeline_.start(buildRealSenseConfig(config_));
    spdlog::info("RealSense pipeline started for calibration");
  } catch (const rs2::error& err) {
    spdlog::error("Failed to start RealSense pipeline: {}", err.what());
    return false;
  }

  try {
    rs2::video_stream_profile color_profile =
        profile_.get_stream(RS2_STREAM_COLOR).as<rs2::video_stream_profile>();
    rs2_intrinsics intr = color_profile.get_intrinsics();

    camera_intrinsics_.fx = intr.fx;
    camera_intrinsics_.fy = intr.fy;
    camera_intrinsics_.cx = intr.ppx;
    camera_intrinsics_.cy = intr.ppy;
    camera_intrinsics_.distortion_model = distortionModelToString(intr.model);

    camera_matrix_ = (cv::Mat_<double>(3, 3) << intr.fx, 0.0, intr.ppx, 0.0, intr.fy,
                      intr.ppy, 0.0, 0.0, 1.0);
    dist_coeffs_ = cv::Mat::zeros(1, 5, CV_64F);
    for (size_t i = 0; i < camera_intrinsics_.distortion_coeffs.size(); ++i) {
      double coeff = (i < 5) ? static_cast<double>(intr.coeffs[i]) : 0.0;
      dist_coeffs_.at<double>(0, static_cast<int>(i)) = coeff;
      camera_intrinsics_.distortion_coeffs[i] = coeff;
    }
    intrinsics_loaded_ = true;
    spdlog::info("Loaded color intrinsics fx={:.2f}, fy={:.2f}, cx={:.2f}, cy={:.2f}",
                 camera_intrinsics_.fx, camera_intrinsics_.fy, camera_intrinsics_.cx,
                 camera_intrinsics_.cy);
  } catch (const rs2::error& err) {
    intrinsics_loaded_ = false;
    spdlog::warn("Failed to query color intrinsics: {}", err.what());
  }

  try {
    rs2::device device = profile_.get_device();
    rs2::depth_sensor depth_sensor = device.first<rs2::depth_sensor>();
    depth_scale_m_ = depth_sensor.get_depth_scale();
    spdlog::info("Depth scale: {:.6f} meters per unit", depth_scale_m_);
  } catch (const rs2::error& err) {
    spdlog::warn("Failed to query depth scale: {}", err.what());
  }
  return true;
}

std::optional<CalibrationSnapshot> CalibrationPipeline::runOnce() {
  FrameBundle bundle;
  if (!captureAlignedFrame(bundle)) {
    spdlog::warn("Failed to capture frame for calibration");
    return std::nullopt;
  }

  auto detection =
      charuco_detector_ ? charuco_detector_->Detect(bundle.color) : std::nullopt;
  if (!detection) {
    spdlog::info("ChArUco board not detected in current frame");
    return std::nullopt;
  }

  CalibrationSnapshot snapshot;
  snapshot.intrinsics = camera_intrinsics_;
  snapshot.detected_charuco_corners = detection->detected_charuco_corners;
  if (!computeHomography(detection->image_points, detection->board_points,
                         snapshot.homography_color_to_position,
                         snapshot.reprojection_error)) {
    spdlog::warn("Homography solve failed or reprojection error too large");
    return std::nullopt;
  }

  if (config_.enable_floor_plane_fit) {
    if (!estimateFloorPlane(bundle, snapshot.floor_plane, snapshot.floor_plane_std_mm,
                            snapshot.inlier_ratio)) {
      spdlog::warn("Floor plane estimation failed");
      snapshot.floor_plane = {0.0F, 0.0F, 1.0F, 0.0F};
      snapshot.floor_plane_std_mm = 0.0;
      snapshot.inlier_ratio = 0.0;
    }
  }

  snapshot.timestamp = std::chrono::system_clock::now();
  return snapshot;
}

bool CalibrationPipeline::captureAlignedFrame(FrameBundle& bundle) {
  try {
    rs2::frameset frames = pipeline_.wait_for_frames();
    frames = align_to_color_.process(frames);

    rs2::video_frame color = frames.get_color_frame();
    rs2::depth_frame depth = frames.get_depth_frame();

    bundle.timestamp_ms = color.get_timestamp();
    bundle.color = cv::Mat(cv::Size(color.get_width(), color.get_height()),
                           CV_8UC3, const_cast<void*>(color.get_data()),
                           cv::Mat::AUTO_STEP)
                       .clone();
    if (intrinsics_loaded_ && !camera_matrix_.empty() && !dist_coeffs_.empty()) {
      cv::Mat undistorted;
      cv::undistort(bundle.color, undistorted, camera_matrix_, dist_coeffs_);
      bundle.color = std::move(undistorted);
    }
    bundle.depth = cv::Mat(cv::Size(depth.get_width(), depth.get_height()),
                           CV_16UC1, const_cast<void*>(depth.get_data()),
                           cv::Mat::AUTO_STEP)
                       .clone();
    return true;
  } catch (const rs2::error& err) {
    spdlog::error("RealSense capture error: {}", err.what());
    return false;
  }
}

bool CalibrationPipeline::computeHomography(
    const std::vector<cv::Point2f>& image_corners,
    const std::vector<cv::Point3f>& object_points, cv::Mat& homography,
    double& reprojection_error) {
  if (image_corners.size() < 4) {
    return false;
  }

  std::vector<cv::Point2f> board_points_2d = boardPointsToPositionId(object_points);

  cv::Mat inliers;
  homography = cv::findHomography(image_corners, board_points_2d, cv::RANSAC,
                                  config_.homography_ransac_thresh_px, inliers);
  if (homography.empty()) {
    return false;
  }

  std::vector<cv::Point2f> projected;
  cv::perspectiveTransform(image_corners, projected, homography);
  double total_sq = 0.0;
  for (size_t i = 0; i < projected.size(); ++i) {
    cv::Point2f diff = projected[i] - board_points_2d[i];
    total_sq += diff.dot(diff);
  }
  reprojection_error =
      projected.empty() ? std::numeric_limits<double>::infinity()
                        : std::sqrt(total_sq / static_cast<double>(projected.size()));

  return reprojection_error <= config_.max_reprojection_error_id;
}

bool CalibrationPipeline::estimateFloorPlane(const FrameBundle& bundle,
                                             cv::Vec4f& plane,
                                             double& plane_std_mm,
                                             double& inlier_ratio) {
  if (!floor_estimator_) {
    spdlog::warn("FloorPlaneEstimator is not initialized.");
    return false;
  }

  if (!intrinsics_loaded_ || depth_scale_m_ <= 0.0) {
    spdlog::warn("Skipping floor estimation: intrinsics or depth scale not initialized.");
    return false;
  }
  if (bundle.depth.empty()) {
    spdlog::warn("Floor estimation skipped: depth image is empty.");
    return false;
  }

  auto estimate =
      floor_estimator_->Estimate(bundle.depth, camera_intrinsics_, depth_scale_m_);
  if (!estimate) {
    spdlog::warn("Floor plane estimation returned no result.");
    return false;
  }

  plane = estimate->plane;
  plane_std_mm = estimate->plane_std_mm;
  inlier_ratio = estimate->inlier_ratio;
  return true;
}

std::vector<cv::Point2f> CalibrationPipeline::boardPointsToPositionId(
    const std::vector<cv::Point3f>& object_points) const {
  std::vector<cv::Point2f> result;
  result.reserve(object_points.size());

  if (!has_playmat_layout_) {
    if (!warned_layout_not_loaded_) {
      spdlog::warn("Playmat layout not loaded. Returning raw Charuco coordinates.");
      warned_layout_not_loaded_ = true;
    }
    for (const auto& pt : object_points) {
      result.emplace_back(pt.x, pt.y);
    }
    return result;
  }

  for (const auto& pt : object_points) {
    cv::Point2f mapped =
        playmat_layout_.TransformBoardPoint(config_.board_mount_label, pt);
    result.emplace_back(mapped);
  }
  return result;
}

}  // namespace locomotion::calibration
