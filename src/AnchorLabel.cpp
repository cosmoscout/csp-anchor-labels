////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "AnchorLabel.hpp"

#include "../../../src/cs-core/GuiManager.hpp"
#include "../../../src/cs-core/InputManager.hpp"
#include "../../../src/cs-core/SolarSystem.hpp"
#include "../../../src/cs-core/TimeControl.hpp"
#include "../../../src/cs-gui/GuiItem.hpp"
#include "../../../src/cs-gui/WorldSpaceGuiArea.hpp"
#include "../../../src/cs-scene/CelestialAnchorNode.hpp"
#include "../../../src/cs-scene/CelestialBody.hpp"
#include "../../../src/cs-utils/FrameTimings.hpp"

#include <GL/glew.h>
#include <VistaKernel/GraphicsManager/VistaGraphicsManager.h>
#include <VistaKernel/GraphicsManager/VistaOpenGLNode.h>
#include <VistaKernel/GraphicsManager/VistaSceneGraph.h>
#include <VistaKernel/GraphicsManager/VistaTransformNode.h>
#include <VistaKernel/VistaSystem.h>
#include <VistaKernelOpenSGExt/VistaOpenSGMaterialTools.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

namespace csp::anchorlabels {

////////////////////////////////////////////////////////////////////////////////////////////////////

AnchorLabel::AnchorLabel(cs::scene::CelestialBody const* const body,
    std::shared_ptr<cs::core::SolarSystem> const&              solarSystem,
    std::shared_ptr<cs::core::GuiManager> const&               guiManager,
    std::shared_ptr<cs::core::TimeControl> const&              timeControl,
    std::shared_ptr<cs::core::InputManager> const&             inputManager)
    : mBody(body)
    , mSolarSystem(solarSystem)
    , mGuiManager(guiManager)
    , mTimeControl(timeControl)
    , mInputManager(inputManager)
    , mGuiArea(std::make_unique<cs::gui::WorldSpaceGuiArea>(120, 30))
    , mGuiItem(
          std::make_unique<cs::gui::GuiItem>("file://../share/resources/gui/anchor_label.html")) {
  auto sceneGraph = GetVistaSystem()->GetGraphicsManager()->GetSceneGraph();

  mAnchor = std::make_shared<cs::scene::CelestialAnchorNode>(sceneGraph->GetRoot(),
      sceneGraph->GetNodeBridge(), "", mBody->getCenterName(), mBody->getFrameName());

  if (mBody->getIsInExistence()) {
    mSolarSystem->registerAnchor(mAnchor);
  }

  mGuiTransform = sceneGraph->NewTransformNode(mAnchor.get());
  mGuiTransform->SetScale(1.0f,
      static_cast<float>(mGuiArea->getHeight()) / static_cast<float>(mGuiArea->getWidth()), 1.0f);
  mGuiTransform->SetTranslation(0.0f, pLabelOffset.get(), 0.0f);
  mGuiTransform->Rotate(VistaAxisAndAngle(VistaVector3D(0.0, 1.0, 0.0), -glm::pi<float>() / 2.f));

  mGuiNode = sceneGraph->NewOpenGLNode(mGuiTransform, mGuiArea.get());
  mInputManager->registerSelectable(mGuiNode);

  mGuiArea->addItem(mGuiItem.get());
  mGuiArea->setUseLinearDepthBuffer(true);
  mGuiArea->setIgnoreDepth(false);

  mGuiItem->setCanScroll(false);
  mGuiItem->waitForFinishedLoading();

  mGuiItem->registerCallback(
      "flyToBody", "Makes the observer fly to the planet marked by this anchor label.", [this] {
        mSolarSystem->flyObserverTo(mBody->getCenterName(), mBody->getFrameName(), 5.0);
        mGuiManager->showNotification("Travelling", "to " + mBody->getCenterName(), "send");
      });

  mGuiItem->callJavascript("setLabelText", mBody->getCenterName());

  pLabelOffset.onChange().connect(
      [this](float newOffset) { mGuiTransform->SetTranslation(0.0f, newOffset, 0.0f); });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

AnchorLabel::~AnchorLabel() {
  mGuiItem->unregisterCallback("flyToBody");

  mSolarSystem->unregisterAnchor(mAnchor);
  mGuiArea->removeItem(mGuiItem.get());

  pLabelOffset.onChange().disconnectAll();
  pLabelOffset.disconnect();

  pLabelScale.disconnect();
  pDepthScale.disconnect();

  delete mGuiNode;
  delete mGuiTransform;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void AnchorLabel::update() const {
  if (mBody->getIsInExistence()) {
    double distanceToObserver = distanceToCamera();
    double simulationTime(mTimeControl->pSimulationTime.get());

    double scale = mSolarSystem->getObserver().getAnchorScale();
    scale *= glm::pow(distanceToObserver, pDepthScale.get()) * pLabelScale.get() * 0.05;
    mAnchor->setAnchorScale(scale);

    cs::scene::CelestialAnchor rawAnchor(mAnchor->getCenterName(), mAnchor->getFrameName());
    rawAnchor.setAnchorPosition(mAnchor->getAnchorPosition());

    auto observerTransform =
        rawAnchor.getRelativeTransform(simulationTime, mSolarSystem->getObserver());
    glm::dvec3 observerPos = observerTransform[3];
    glm::dvec3 y           = observerTransform * glm::dvec4(0, 1, 0, 0);
    glm::dvec3 camDir      = glm::normalize(observerPos);

    glm::dvec3 z = glm::cross(y, camDir);
    glm::dvec3 x = glm::cross(y, z);

    x = glm::normalize(x);
    y = glm::normalize(y);
    z = glm::normalize(z);

    auto rot = glm::toQuat(glm::dmat3(x, y, z));
    mAnchor->setAnchorRotation(rot);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

glm::dvec4 AnchorLabel::getScreenSpaceBB() const {
  const float width  = pLabelScale.get() * mGuiArea->getWidth() * 0.0005;
  const float height = pLabelScale.get() * mGuiArea->getHeight() * 0.0005;

  glm::dvec3 pos =
      mSolarSystem->getObserver().getRelativePosition(mTimeControl->pSimulationTime(), *mAnchor);
  const auto screenPos = (pos.xyz() / pos.z).xy();

  const double x = screenPos.x - (width / 2.0);
  const double y = screenPos.y - (height / 2.0);

  return {x, y, width, height};
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void AnchorLabel::enable() const {
  mGuiItem->setIsEnabled(true);
  mSolarSystem->registerAnchor(mAnchor);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void AnchorLabel::disable() const {
  mGuiItem->setIsEnabled(false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::string const& AnchorLabel::getCenterName() const {
  return mBody->getCenterName();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool AnchorLabel::shouldBeHidden() const {
  return !mBody->getIsInExistence();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

double AnchorLabel::bodySize() const {
  return mBody->pVisibleRadius();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

double AnchorLabel::distanceToCamera() const {
  double simulationTime(mTimeControl->pSimulationTime.get());
  return glm::length(mSolarSystem->getObserver().getRelativePosition(simulationTime, *mAnchor));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void AnchorLabel::setSortKey(int key) const {
  VistaOpenSGMaterialTools::SetSortKeyOnSubtree(mGuiTransform, key);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::anchorlabels
