////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Plugin.hpp"
#include "AnchorLabel.hpp"

#include "../../../src/cs-core/GuiManager.hpp"
#include "../../../src/cs-core/PluginBase.hpp"
#include "../../../src/cs-core/SolarSystem.hpp"
#include "../../../src/cs-utils/utils.hpp"

#include <iostream>
#include <json.hpp>

////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT_FN cs::core::PluginBase* create() {
  return new csp::anchorlabels::Plugin;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT_FN void destroy(cs::core::PluginBase* pluginBase) {
  delete pluginBase;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace csp::anchorlabels {

////////////////////////////////////////////////////////////////////////////////////////////////////

void from_json(const nlohmann::json& j, Plugin::Settings& o) {
  cs::core::parseSection("csp-anchor-labels", [&] {
    o.mDefaultEnabled         = cs::core::parseProperty<bool>("defaultEnabled", j);
    o.mEnableDepthOverlap     = cs::core::parseProperty<bool>("enableDepthOverlap", j);
    o.mIgnoreOverlapThreshold = cs::core::parseProperty<double>("ignoreOverlapThreshold", j);
    o.mLabelScale             = cs::core::parseProperty<double>("labelScale", j);
    o.mDepthScale             = cs::core::parseProperty<double>("depthScale", j);
    o.mLabelOffset            = cs::core::parseProperty<float>("labelOffset", j);
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Plugin::Plugin()
    : PluginBase() {
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::init() {
  std::cout << "Loading: CosmoScout VR Plugin Anchor Labels" << std::endl;

  Settings settings = mAllSettings->mPlugins.at("csp-anchor-labels");

  pEnabled                = settings.mDefaultEnabled;
  pEnableDepthOverlap     = settings.mEnableDepthOverlap;
  pIgnoreOverlapThreshold = settings.mIgnoreOverlapThreshold;
  pLabelScale             = settings.mLabelScale;
  pDepthScale             = settings.mDepthScale;
  pLabelOffset            = settings.mLabelOffset;

  mGuiManager->addSettingsSectionToSideBarFromHTML(
      "Anchor Labels", "location_on", "../share/resources/gui/anchor_labels_settings.html");

    mGuiManager->addScriptToGuiFromJS("../share/resources/gui/js/anchor_labels_settings.js");

  // create labels for all bodies that already exist
  for (auto const& body : mSolarSystem->getBodies()) {
    mAnchorLabels.push_back(std::make_unique<AnchorLabel>(
        body.get(), mSolarSystem, mGuiManager, mTimeControl, mInputManager));
  }

  // for all bodies that will be created in the future we also create a label
  addListenerId = mSolarSystem->registerAddBodyListener([this](auto const& body) {
    mAnchorLabels.push_back(std::make_unique<AnchorLabel>(
        body.get(), mSolarSystem, mGuiManager, mTimeControl, mInputManager));

    // this feels hacky o.O
    auto newLabel = mAnchorLabels.at(mAnchorLabels.size() - 1).get();

    newLabel->pLabelOffset.connectFrom(pLabelOffset);
    newLabel->pLabelScale.connectFrom(pLabelScale);
    newLabel->pDepthScale.connectFrom(pDepthScale);

    mNeedsResort = true;
  });

  // if a body gets dropped from the solar system remove the label too
  removeListenerId = mSolarSystem->registerRemoveBodyListener([this](auto const& body) {
    std::remove_if(mAnchorLabels.begin(), mAnchorLabels.end(),
        [body](auto const& label) { return body->getCenterName() == label->getCenterName(); });
  });

  mGuiManager->getGui()->registerCallback<bool>(
      "set_enable_anchor_labels", [this](bool value) { pEnabled = value; });

  mGuiManager->getGui()->registerCallback<bool>(
      "set_enable_depth_overlap", [this](bool value) { pEnableDepthOverlap = value; });

  mGuiManager->getGui()->registerCallback<double>("set_anchor_label_ignore_overlap_threshold",
      ([this](double value) { pIgnoreOverlapThreshold = value; }));

  mGuiManager->getGui()->registerCallback<double>(
      "set_anchor_label_scale", ([this](double value) { pLabelScale = value; }));

  mGuiManager->getGui()->registerCallback<double>(
      "set_anchor_label_depth_scale", ([this](double value) { pDepthScale = value; }));

  mGuiManager->getGui()->registerCallback<double>(
      "set_anchor_label_offset", ([this](double value) { pLabelOffset = value; }));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::update() {
  if (mNeedsResort) {
    std::sort(mAnchorLabels.begin(), mAnchorLabels.end(),
        [](std::unique_ptr<AnchorLabel>& l1, std::unique_ptr<AnchorLabel>& l2) {
          return l1->bodySize() > l2->bodySize();
        });
    mNeedsResort = false;
  }

  std::unordered_set<AnchorLabel*> labelsToDraw;
  if (pEnabled.get()) {

    for (auto const& label : mAnchorLabels) {
      if (label->shouldBeHidden()) {
        continue;
      }

      glm::dvec4 A             = label->getScreenSpaceBB();
      double     distToCameraA = label->distanceToCamera();

      bool canBeAdded = true;
      for (auto&& drawLabel : labelsToDraw) {
        if (pEnableDepthOverlap.get()) {
          // Check the distance relative to each other. If they are far apart we can display both.
          double distToCameraB    = drawLabel->distanceToCamera();
          double relativeDistance = distToCameraA < distToCameraB ? distToCameraB / distToCameraA
                                                                  : distToCameraA / distToCameraB;
          if (relativeDistance > 1 + pIgnoreOverlapThreshold.get() * 0.1) {
            continue;
          }
        }

        // Check if they are colliding. If they collide the bigger label survives. Since the list
        // is sorted by body size, it is assured that the bigger label gets displayed.
        glm::dvec4 B   = drawLabel->getScreenSpaceBB();
        bool collision = B.x + B.z > A.x && B.y + B.w > A.y && A.x + A.z > B.x && A.y + A.w > B.y;
        if (collision) {
          canBeAdded = false;
          break;
        }
      }

      if (canBeAdded) {
        labelsToDraw.insert(label.get());
      }
    }
  }

  for (auto&& label : mAnchorLabels) {
    if (labelsToDraw.find(label.get()) != labelsToDraw.end()) {
      label->update();
      label->enable();
    } else {
      label->disable();
    }
  }

  std::vector<AnchorLabel*> sortedLabels(labelsToDraw.begin(), labelsToDraw.end());
  std::sort(sortedLabels.begin(), sortedLabels.end(),
      [](AnchorLabel* a, AnchorLabel* b) { return a->distanceToCamera() < b->distanceToCamera(); });

  for (int i = 0; i < sortedLabels.size(); ++i) {
    // a little bit hacky... It probably breaks, when more than 100 labels are present.
    sortedLabels[i]->setSortKey(static_cast<int>(cs::utils::DrawOrder::eTransparentItems) - i);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::deInit() {
  mAnchorLabels.clear();

  mSolarSystem->unregisterAddBodyListener(addListenerId);
  mSolarSystem->unregisterRemoveBodyListener(removeListenerId);

  mGuiManager->getGui()->unregisterCallback("set_enable_anchor_labels");
  mGuiManager->getGui()->unregisterCallback("set_enable_depth_overlap");
  mGuiManager->getGui()->unregisterCallback("set_anchor_label_ignore_overlap_threshold");
  mGuiManager->getGui()->unregisterCallback("set_anchor_label_scale");
  mGuiManager->getGui()->unregisterCallback("set_anchor_label_depth_scale");
  mGuiManager->getGui()->unregisterCallback("set_anchor_label_offset");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::anchorlabels
