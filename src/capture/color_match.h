#pragma once

#include <cstdint>
#include "screen_capture.h"

namespace aimlab {

struct TargetMatch {
    bool found = false;
    int  x     = 0;   // pixel coord
    int  y     = 0;
    int  size  = 0;   // number of matching pixels in the cluster window
};

// Finds the matching pixel that sits at the densest point of a color blob
// (i.e. the visual center) closest to (anchorX, anchorY). A pixel matches
// when its squared RGB distance to (tr,tg,tb) is <= tol^2 * 3.
//
//   * minBlobPx     — fast 3x3 neighbor pre-filter (rejects isolated noise)
//   * clusterRadius — the radius of the cluster window used to score pixels
//   * minCluster    — minimum number of matches inside that window
//
// The candidate with the highest cluster count wins; ties break by closeness
// to the anchor. This naturally prefers pixels in the middle of a colored
// region over pixels on its edge.
//
// Sticky hint (preferredX, preferredY, stickRadius): if preferredX/Y is
// valid (>= 0) and a candidate lies within stickRadius of the hint, that
// candidate wins regardless of anchor distance. This implements "target
// lock" — when several blobs are visible (e.g. 3 dots in a circle), the
// caller commits to one and FindTarget will keep returning it as long as
// the blob stays within stickRadius of the previous position. If no
// candidate is close to the hint, the call falls back to the normal
// "closest to anchor" rule.
TargetMatch FindTarget(const ScreenFrame& frame,
                       int anchorX, int anchorY,
                       int tr, int tg, int tb,
                       int tolerance,
                       int minBlobPx,
                       int clusterRadius,
                       int minCluster,
                       int preferredX = -1,
                       int preferredY = -1,
                       int stickRadius = 60);

}  // namespace aimlab
