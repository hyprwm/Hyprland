#ifdef HYPR_USE_CAPTURE_ATTACHMENT
layout(location = 1) out vec4 fragColorCapture;
uniform int capture;

#define CAPTURE_WRITE(v) fragColorCapture = mix((v), vec4(0.0, 0.0, 0.0, (v).a), float(capture))
#else
#define CAPTURE_WRITE(v)
#endif
