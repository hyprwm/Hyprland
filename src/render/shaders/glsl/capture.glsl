#ifdef HYPR_USE_CAPTURE_ATTACHMENT
uniform int  capture;
uniform vec4 captureColor;
layout(location = 1) out vec4 fragColorCapture;

// a branchless way of checking alpha > 0.0
#define CAPTURE_WRITE(v) fragColorCapture = mix((v), vec4(captureColor.rgb * captureColor.a, captureColor.a), float(capture) * step(0.001, (v).a))
#else
#define CAPTURE_WRITE(v)
#endif
