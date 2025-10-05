#ifdef HYPR_USE_CAPTURE_ATTACHMENT
uniform int capture;
layout(location = 1) out vec4 fragColorCapture;

#define CAPTURE_WRITE(v) fragColorCapture = mix((v), vec4(0.0, 0.0, 0.0, (v).a), float(capture))
#else
#define CAPTURE_WRITE(v)
#endif
