#include "BinningTestTypes.h"

#include <cmath>

bool isSupportedBinningFactor(int factor)
{
    return factor == 1 || factor == 2 || factor == 4;
}

QSize expectedBinningSize(const QSize& baseline, int factor)
{
    if (!baseline.isValid() || !isSupportedBinningFactor(factor)) return QSize();
    return QSize(baseline.width() / factor, baseline.height() / factor);
}

bool binningFeatureWidthMatches(int baselinePixels, int actualPixels,
                                int factor, int tolerancePixels)
{
    if (baselinePixels < 0 || actualPixels < 0 || factor <= 0 || tolerancePixels < 0) {
        return false;
    }
    const double expected = static_cast<double>(baselinePixels) / factor;
    return std::abs(static_cast<double>(actualPixels) - expected) <= tolerancePixels;
}
