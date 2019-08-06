////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CSP_ANCHOR_LABELS_PLUGIN_HPP
#define CSP_ANCHOR_LABELS_PLUGIN_HPP

#include "../../../src/cs-core/PluginBase.hpp"
#include "../../../src/cs-core/Settings.hpp"
#include "../../../src/cs-utils/Property.hpp"
#include <memory>
#include <vector>

namespace csp::anchorlabels {
class AnchorLabel;

/// This plugin puts labels over anchors in space. It uses the anchors center names as text. If
/// you click on the label you ar being flown to the anchor. The plugin is configurable via the
/// application config file. See README.md for details.
class Plugin : public cs::core::PluginBase {
 public:
  struct Settings {
    bool   mDefaultEnabled;
    bool   mEnableDepthOverlap;
    double mIgnoreOverlapThreshold;
    double mLabelScale;
    double mDepthScale;
    float  mLabelOffset;
  };

  cs::utils::Property<bool> pEnabled = true;

  /// If true the labels will ignore depth for collision.
  cs::utils::Property<bool> pEnableDepthOverlap = true;

  cs::utils::Property<double> pLabelScale = 0.1; ///< The general size of the anchor labels.

  /// A factor that determines how much smaller further away labels are. With a value of 1.0 all
  /// labels are the same size regardless of distance from the observer, with a value smaller than
  /// 1.0 the farther away labels are smaller than the nearer ones.
  cs::utils::Property<double> pDepthScale = 0.85;

  /// The value describes the labels height over the anchor.
  cs::utils::Property<float> pLabelOffset = 0.2f;

  /// Determines when labels are drawn, even if they overlap on screen. The value represents a
  /// threshold, which is dependent on the distance of the two colliding labels. If the relative
  /// difference in distance to the camera exceeds this threshold the labels are drawn anyways.
  ///
  /// E.g.: PlanetA is 100 units away, PlanetB is 120 units away and the value is smaller than 0.2.
  ///       Both labels will display, because their relative distance between them is smaller than
  ///       the threshold.
  cs::utils::Property<double> pIgnoreOverlapThreshold = 0.025;

  Plugin();
  void init() override;
  void deInit() override;
  void update() override;

 private:
  bool mNeedsResort = true; ///< When a new label gets added resort the vector
  std::vector<std::unique_ptr<AnchorLabel>> mAnchorLabels;

  uint64_t addListenerId{};
  uint64_t removeListenerId{};
};
} // namespace csp::anchorlabels

#endif // CSP_ANCHOR_LABELS_PLUGIN_HPP