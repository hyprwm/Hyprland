#include "AlphaModifiable.hpp"

using namespace Desktop::View;

bool IAlphaModifiable::alphaNonZero() const {
    return alpha().getTotal() > 0.F;
}
