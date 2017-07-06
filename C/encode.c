#include "encode.h"

#include <string.h>
#include <math.h>

static float *multiplyBasisFunction(int xComponent, int yComponent, int width, int height, uint8_t *rgb, size_t bytesPerRow);
static char *encode_int(int value, int length, char *destination);

static int linearToGamma(float value);
static float gammaToLinear(int value);
static int encodeDC(float r, float g, float b);
static int encodeAC(float r, float g, float b, float maximumValue);
static float signPow(float value, float exp);

const char *encode(int xComponents, int yComponents, int width, int height, uint8_t *rgb, size_t bytesPerRow) {
    static char buffer[2 + 4 + (8 * 8 - 1) * 2 + 1];

    if(xComponents < 1 || xComponents > 8) return NULL;
    if(yComponents < 1 || yComponents > 8) return NULL;

    float factors[yComponents][xComponents][3];
    memset(factors, 0, sizeof(factors));

    for(int y = 0; y < yComponents; y++) {
        for(int x = 0; x < xComponents; x++) {
            float *factor = multiplyBasisFunction(x, y, width, height, rgb, bytesPerRow);
            factors[y][x][0] = factor[0];
            factors[y][x][1] = factor[1];
            factors[y][x][2] = factor[2];
        }
    }

    float *dc = factors[0][0];
    float *ac = dc + 3;
    int acCount = xComponents * yComponents - 1;
    char *ptr = buffer;

    int sizeFlag = (xComponents - 1) + ((yComponents - 1) << 3);
    ptr = encode_int(sizeFlag, 1, ptr);

    float maximumValue;
    if(acCount > 0) {
        float actualMaximumValue = 0;
        for(int i = 0; i < acCount * 3; i++) {
            actualMaximumValue = fmaxf(ac[i], actualMaximumValue);
        }

        int quantisedMaximumValue = fmaxf(0, fminf(63, floorf(actualMaximumValue * 128 - 0.5)));
        maximumValue = ((float)quantisedMaximumValue + 1) / 128;
        ptr = encode_int(quantisedMaximumValue, 1, ptr);
    } else {
        maximumValue = 1;
        ptr = encode_int(0, 1, ptr);
    }

    ptr = encode_int(encodeDC(dc[0], dc[1], dc[2]), 4, ptr);

    for(int i = 0; i < acCount; i++) {
        ptr = encode_int(encodeAC(ac[i * 3 + 0], ac[i * 3 + 1], ac[i * 3 + 2], maximumValue), 2, ptr);
    }

    *ptr = 0;

    return buffer;
}

static float *multiplyBasisFunction(int xComponent, int yComponent, int width, int height, uint8_t *rgb, size_t bytesPerRow) {
    float r = 0, g = 0, b = 0;

    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            float basis = cosf(M_PI * xComponent * x / width) * cosf(M_PI * yComponent * y / height);
            r += basis * gammaToLinear(rgb[3 * x + 0 + y * bytesPerRow]);
            g += basis * gammaToLinear(rgb[3 * x + 1 + y * bytesPerRow]);
            b += basis * gammaToLinear(rgb[3 * x + 2 + y * bytesPerRow]);
        }
    }

    float scale = width * height;

    static float result[3];
    result[0] = r / scale;
    result[1] = g / scale;
    result[2] = b / scale;

    return result;
}

static int linearToGamma(float value) {
    return floorf(powf(fmaxf(0, fminf(1, value)), 1 / 2.2) * 255) + 0.5;
}

static float gammaToLinear(int value) {
    return powf((float)value / 255, 2.2);
}

static int encodeDC(float r, float g, float b) {
    int roundedR = linearToGamma(r);
    int roundedG = linearToGamma(g);
    int roundedB = linearToGamma(b);
    return (roundedR << 16) + (roundedG << 8) + roundedB;
}

static int encodeAC(float r, float g, float b, float maximumValue) {
    int quantR = fmaxf(0, fminf(15, floorf(signPow(r / maximumValue, 0.333) * 8 + 8.5)));
    int quantG = fmaxf(0, fminf(15, floorf(signPow(g / maximumValue, 0.333) * 8 + 8.5)));
    int quantB = fmaxf(0, fminf(15, floorf(signPow(b / maximumValue, 0.333) * 8 + 8.5)));

    return (quantR << 8) + (quantG << 4) + quantB;
}

static float signPow(float value, float exp) {
    return copysignf(powf(fabsf(value), exp), value);
}

static char characters[64]="0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ:;";

static char *encode_int(int value, int length, char *destination) {
    for(int i = 1; i <= length; i++) {
        char digit = (value >> (6 * (length - i))) & 63;
        *destination++ = characters[digit];
    }
    return destination;
}
