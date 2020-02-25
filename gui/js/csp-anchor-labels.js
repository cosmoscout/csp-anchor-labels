/* global IApi, CosmoScout */

/**
 * Atmosphere Api
 */
class AnchorLabelApi extends IApi {
  /**
   * @inheritDoc
   */
  name = 'anchorLabel';

  /**
   * @inheritDoc
   */
  init() {
    CosmoScout.gui.initSlider('setAnchorLabelIgnoreOverlapThreshold', 0.0, 0.2, 0.001, [0.1]);
    CosmoScout.gui.initSlider('setAnchorLabelScale', 0.1, 5.0, 0.01, [1.2]);
    CosmoScout.gui.initSlider('setAnchorLabelDepthScale', 0.0, 1.0, 0.01, [1.0]);
    CosmoScout.gui.initSlider('setAnchorLabelOffset', 0.0, 1.0, 0.01, [0.2]);
  }
}

(() => {
  CosmoScout.init(AnchorLabelApi);
})();
