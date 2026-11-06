// IMA ADPCM: speech squeezed to four bits a sample (voice chat: 16 kHz mono is 64 kbit/s). Every packet states the
// predictor and step index it starts from, so a lost packet never spoils the next one.
#ifndef _BC_ADPCM
#define _BC_ADPCM

#include <stdint.h>

namespace adpcm {

static const int STEP[89] = { 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767 };
static const int INDEX[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/// samples: n (even) signed 16-bit values; out: n/2 bytes. pred and idx are the state at the start and are left at the end.
inline void Encode(const int16_t *samples, int n, int &pred, int &idx, uint8_t *out) {
	for (int i = 0; i < n / 2; i++) out[i] = 0;
	for (int i = 0; i < n; i++) {
		int step = STEP[idx];
		int diff = samples[i] - pred, code = 0;
		if (diff < 0) { code = 8; diff = -diff; }
		int t = step;
		if (diff >= t) { code |= 4; diff -= t; }
		t >>= 1; if (diff >= t) { code |= 2; diff -= t; }
		t >>= 1; if (diff >= t) code |= 1;
		int dq = step >> 3;
		if (code & 4) dq += step;
		if (code & 2) dq += step >> 1;
		if (code & 1) dq += step >> 2;
		pred = clampi((code & 8) ? pred - dq : pred + dq, -32768, 32767);
		idx = clampi(idx + INDEX[code & 7], 0, 88);
		out[i >> 1] |= (uint8_t)((i & 1) ? code << 4 : code);
	}
}

/// bytes: n/2 packed codes; out: n samples in -1..1
inline void Decode(const uint8_t *bytes, int n, int pred, int idx, float *out) {
	pred = clampi(pred, -32768, 32767);
	idx = clampi(idx, 0, 88);
	for (int i = 0; i < n; i++) {
		int code = (bytes[i >> 1] >> ((i & 1) ? 4 : 0)) & 15, step = STEP[idx];
		int dq = step >> 3;
		if (code & 4) dq += step;
		if (code & 2) dq += step >> 1;
		if (code & 1) dq += step >> 2;
		pred = clampi((code & 8) ? pred - dq : pred + dq, -32768, 32767);
		idx = clampi(idx + INDEX[code & 7], 0, 88);
		out[i] = pred / 32768.0f;
	}
}

}

#endif
