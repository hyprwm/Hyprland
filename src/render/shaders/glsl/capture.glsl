#ifdef HYPR_USE_CAPTURE_ATTACHMENT
layout(location = 1) out vec4 fragColorCapture;
#define CAPTURE_WRITE(v) fragColorCapture = (v);
#else
#define CAPTURE_WRITE(v)
#endif
