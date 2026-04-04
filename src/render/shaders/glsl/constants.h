#ifndef CONSTANTS_H
#define CONSTANTS_H
//enum eTransferFunction
#define CM_TRANSFER_FUNCTION_BT1886     1
#define CM_TRANSFER_FUNCTION_GAMMA22    2
#define CM_TRANSFER_FUNCTION_GAMMA28    3
#define CM_TRANSFER_FUNCTION_ST240      4
#define CM_TRANSFER_FUNCTION_EXT_LINEAR 5
#define CM_TRANSFER_FUNCTION_LOG_100    6
#define CM_TRANSFER_FUNCTION_LOG_316    7
#define CM_TRANSFER_FUNCTION_XVYCC      8
#define CM_TRANSFER_FUNCTION_SRGB       9
#define CM_TRANSFER_FUNCTION_EXT_SRGB   10
#define CM_TRANSFER_FUNCTION_ST2084_PQ  11
#define CM_TRANSFER_FUNCTION_ST428      12
#define CM_TRANSFER_FUNCTION_HLG        13

// sRGB constants
#define SRGB_POW   2.4
#define SRGB_CUT   0.0031308
#define SRGB_SCALE 12.92
#define SRGB_ALPHA 1.055

#define BT1886_POW   (1.0 / 0.45)
#define BT1886_CUT   0.018053968510807
#define BT1886_SCALE 4.5
#define BT1886_ALPHA (1.0 + 5.5 * BT1886_CUT)

// See http://car.france3.mars.free.fr/HD/INA-%2026%20jan%2006/SMPTE%20normes%20et%20confs/s240m.pdf
#define ST240_POW   (1.0 / 0.45)
#define ST240_CUT   0.0228
#define ST240_SCALE 4.0
#define ST240_ALPHA 1.1115

#define ST428_POW   2.6
#define ST428_SCALE (52.37 / 48.0)

// PQ constants
#define PQ_M1     0.1593017578125
#define PQ_M2     78.84375
#define PQ_INV_M1 (1.0 / PQ_M1)
#define PQ_INV_M2 (1.0 / PQ_M2)
#define PQ_C1     0.8359375
#define PQ_C2     18.8515625
#define PQ_C3     18.6875

// HLG constants
#define HLG_D_CUT (1.0 / 12.0)
#define HLG_E_CUT 0.5
#define HLG_A     0.17883277
#define HLG_B     0.28466892
#define HLG_C     0.55991073

#define SDR_MIN_LUMINANCE 0.2
#define SDR_MAX_LUMINANCE 80.0
#define HDR_MIN_LUMINANCE 0.005
#define HDR_MAX_LUMINANCE 10000.0
#define HLG_MAX_LUMINANCE 1000.0

#define M_E 2.718281828459045

#endif
