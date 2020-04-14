////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CSP_ANCHOR_LABELS_ANCHOR_LABEL_HPP
#define CSP_ANCHOR_LABELS_ANCHOR_LABEL_HPP

#include "../../../src/cs-scene/CelestialBody.hpp"
#include "../../../src/cs-utils/Property.hpp"
#include <memory>

class VistaOpenGLNode;
class VistaTransformNode;

namespace cs::scene {
class CelestialBody;
class CelestialAnchorNode;
} // namespace cs::scene

namespace cs::gui {
class WorldSpaceGuiArea;
class GuiItem;
} // namespace cs::gui

namespace cs::core {
class SolarSystem;
class GuiManager;
class TimeControl;
class InputManager;
} // namespace cs::core

namespace csp::anchorlabels {
class AnchorLabel {
 public:
  /// The general size of the anchor labels.
  cs::utils::Property<double> pLabelScale = 0.1;

  /// A factor that determines how much smaller further away labels are. With a value of 1.0 all
  /// labels are the same size regardless of distance from the observer, with a value smaller than
  /// 1.0 the farther away labels are smaller than the nearer ones.
  cs::utils::Property<double> pDepthScale = 0.85;

  /// The value describes the labels height over the anchor.
  cs::utils::Property<float> pLabelOffset = 0.2f;

  AnchorLabel(cs::scene::CelestialBody const*        body,
      std::shared_ptr<cs::core::SolarSystem> const&  solarSystem,
      std::shared_ptr<cs::core::GuiManager> const&   guiManager,
      std::shared_ptr<cs::core::TimeControl> const&  timeControl,
      std::shared_ptr<cs::core::InputManager> const& inputManager);

  ~AnchorLabel();

  void update();

  std::string const& getCenterName() const;

  bool   shouldBeHidden() const;
  double bodySize() const;
  double distanceToCamera() const;

  void setSortKey(int key) const;

  void enable() const;
  void disable() const;

  glm::dvec4 getScreenSpaceBB() const;

 private:
  cs::scene::CelestialBody const* const mBody;

  std::shared_ptr<cs::core::SolarSystem>  mSolarSystem;
  std::shared_ptr<cs::core::GuiManager>   mGuiManager;
  std::shared_ptr<cs::core::TimeControl>  mTimeControl;
  std::shared_ptr<cs::core::InputManager> mInputManager;

  std::shared_ptr<cs::scene::CelestialAnchorNode> mAnchor;

  std::unique_ptr<cs::gui::WorldSpaceGuiArea> mGuiArea;
  std::unique_ptr<cs::gui::GuiItem>           mGuiItem;
  std::unique_ptr<VistaOpenGLNode>            mGuiNode;
  std::unique_ptr<VistaTransformNode>         mGuiTransform;

  glm::dvec3 mRelativeAnchorPosition;
};
} // namespace csp::anchorlabels

#endif // CSP_ANCHOR_LABELS_ANCHOR_LABEL_HPP
