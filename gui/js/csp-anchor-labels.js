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
    CosmoScout.gui.initSlider('set_anchor_label_ignore_overlap_threshold', 0.0, 0.2, 0.001, [0.1]);
    CosmoScout.gui.initSlider('set_anchor_label_scale', 0.1, 5.0, 0.01, [1.2]);
    CosmoScout.gui.initSlider('set_anchor_label_depth_scale', 0.0, 1.0, 0.01, [1.0]);
    CosmoScout.gui.initSlider('set_anchor_label_offset', 0.0, 1.0, 0.01, [0.2]);
  }
}

(() => {
  CosmoScout.init(AnchorLabelApi);
})();
