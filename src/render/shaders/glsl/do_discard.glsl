if (discardOpaque && pixColor.a * alpha == 1.0)
    discard;

if (discardAlpha && pixColor.a <= discardAlphaValue)
    discard;