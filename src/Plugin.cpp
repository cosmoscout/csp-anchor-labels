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
#include "../../../src/cs-utils/logger.hpp"
#include "../../../src/cs-utils/utils.hpp"
#include "logger.hpp"

#include <iostream>
#include <json.hpp>

////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT_FN cs::core::PluginBase* create() {
  return new csp::anchorlabels::Plugin;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT_FN void destroy(cs::core::PluginBase* pluginBase) {
  delete pluginBase; // NOLINT(cppcoreguidelines-owning-memory)
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

void Plugin::init() {

  logger()->info("Loading plugin...");

  Settings settings = mAllSettings->mPlugins.at("csp-anchor-labels");

  pEnabled                = settings.mDefaultEnabled;
  pEnableDepthOverlap     = settings.mEnableDepthOverlap;
  pIgnoreOverlapThreshold = settings.mIgnoreOverlapThreshold;
  pLabelScale             = settings.mLabelScale;
  pDepthScale             = settings.mDepthScale;
  pLabelOffset            = settings.mLabelOffset;

  mGuiManager->addSettingsSectionToSideBarFromHTML(
      "Anchor Labels", "location_on", "../share/resources/gui/anchor_labels_settings.html");

  mGuiManager->addScriptToGuiFromJS("../share/resources/gui/js/csp-anchor-labels.js");

  // create labels for all bodies that already exist
  for (auto const& body : mSolarSystem->getBodies()) {
    mAnchorLabels.emplace_back(std::make_unique<AnchorLabel>(
        body.get(), mSolarSystem, mGuiManager, mTimeControl, mInputManager));

    mAnchorLabels.back()->pLabelOffset.connectFrom(pLabelOffset);
    mAnchorLabels.back()->pLabelScale.connectFrom(pLabelScale);
    mAnchorLabels.back()->pDepthScale.connectFrom(pDepthScale);

    mNeedsResort = true;
  }

  // for all bodies that will be created in the future we also create a label
  addListenerId = mSolarSystem->registerAddBodyListener([this](auto const& body) {
    mAnchorLabels.emplace_back(std::make_unique<AnchorLabel>(
        body.get(), mSolarSystem, mGuiManager, mTimeControl, mInputManager));

    mAnchorLabels.back()->pLabelOffset.connectFrom(pLabelOffset);
    mAnchorLabels.back()->pLabelScale.connectFrom(pLabelScale);
    mAnchorLabels.back()->pDepthScale.connectFrom(pDepthScale);

    mNeedsResort = true;
  });

  // if a body gets dropped from the solar system remove the label too
  removeListenerId = mSolarSystem->registerRemoveBodyListener([this](auto const& body) {
    mAnchorLabels.erase(
        std::remove_if(mAnchorLabels.begin(), mAnchorLabels.end(),
            [body](auto const& label) { return body->getCenterName() == label->getCenterName(); }),
        mAnchorLabels.end());
  });

  mGuiManager->getGui()->registerCallback("anchorLabels.setEnabled",
      "Enables or disables anchor labels.",
      std::function([this](bool value) { pEnabled = value; }));

  mGuiManager->getGui()->registerCallback("anchorLabels.setEnableOverlap",
      "Enables or disables overlapping of anchor labels.",
      std::function([this](bool value) { pEnableDepthOverlap = value; }));

  mGuiManager->getGui()->registerCallback("anchorLabels.setIgnoreOverlapThreshold",
      "Higher values will prevent anchor labels to be hidden when they overlap a little.",
      std::function([this](double value) { pIgnoreOverlapThreshold = value; }));

  mGuiManager->getGui()->registerCallback("anchorLabels.setScale",
      "Sets a global scale multiplier for all anchor labels.",
      std::function([this](double value) { pLabelScale = value; }));

  mGuiManager->getGui()->registerCallback("anchorLabels.setDepthScale",
      "Higher values will make the scale of the anchor labels depend on their distance to the "
      "observer.",
      std::function([this](double value) { pDepthScale = value; }));

  mGuiManager->getGui()->registerCallback("anchorLabels.setOffset",
      "Specifies the distance between planet and anchor labels.",
      std::function([this](double value) { pLabelOffset = static_cast<float>(value); }));

  logger()->info("Loading done.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::update() {
  if (pEnabled.get()) {

    if (mNeedsResort) {
      std::sort(mAnchorLabels.begin(), mAnchorLabels.end(),
          [](std::unique_ptr<AnchorLabel>& l1, std::unique_ptr<AnchorLabel>& l2) {
            return l1->bodySize() > l2->bodySize();
          });
      mNeedsResort = false;
    }

    for (auto&& label : mAnchorLabels) {
      label->update();
    }

    std::unordered_set<AnchorLabel*> labelsToDraw;
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

    for (auto&& label : mAnchorLabels) {
      if (labelsToDraw.find(label.get()) != labelsToDraw.end()) {
        label->enable();
      } else {
        label->disable();
      }
    }

    std::vector<AnchorLabel*> sortedLabels(labelsToDraw.begin(), labelsToDraw.end());
    std::sort(sortedLabels.begin(), sortedLabels.end(), [](AnchorLabel* a, AnchorLabel* b) {
      return a->distanceToCamera() < b->distanceToCamera();
    });

    for (int i = 0; i < static_cast<int>(sortedLabels.size()); ++i) {
      // a little bit hacky... It probably breaks, when more than 100 labels are present.
      sortedLabels[i]->setSortKey(static_cast<int>(cs::utils::DrawOrder::eTransparentItems) - i);
    }
  } else {
    for (auto&& label : mAnchorLabels) {
      label->disable();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::deInit() {
  logger()->info("Unloading plugin...");

  mAnchorLabels.clear();

  mSolarSystem->unregisterAddBodyListener(addListenerId);
  mSolarSystem->unregisterRemoveBodyListener(removeListenerId);

  mGuiManager->removeSettingsSection("Anchor Labels");

  mGuiManager->getGui()->unregisterCallback("anchorLabels.setEnabled");
  mGuiManager->getGui()->unregisterCallback("anchorLabels.setEnableOverlap");
  mGuiManager->getGui()->unregisterCallback("anchorLabels.setIgnoreOverlapThreshold");
  mGuiManager->getGui()->unregisterCallback("anchorLabels.setScale");
  mGuiManager->getGui()->unregisterCallback("anchorLabels.setDepthScale");
  mGuiManager->getGui()->unregisterCallback("anchorLabels.setOffset");

  logger()->info("Unloading done.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::anchorlabels
