/*
 * Points.h
 *
 *  Created on: 4 мая 2021 г.
 *      Author: layst
 */

#ifndef POINTS_H__
#define POINTS_H__

#include <inttypes.h>

#define HOUSES_CNT      4

#define INDX_GRIF        0
#define INDX_SLYZE       1
#define INDX_RAVE        2
#define INDX_HUFF        3

namespace Points {

union Values {
    struct { int32_t grif, slyze, rave, huff; };
    int32_t arr[HOUSES_CNT];
    int32_t Sum() { return grif + slyze + rave + huff; }
    int32_t Max() {
        int32_t         max = grif;
        if(slyze > max) max = slyze;
        if(rave > max)  max = rave;
        if(huff > max)  max = huff;
        return max;
    }
    Values() : grif(0), slyze(0), rave(0), huff(0) {}
    Values(int16_t g, int16_t s, int16_t r, int16_t h) : grif(g), slyze(s), rave(r), huff(h) {}
};

void Init();
void Set(Values v);
void SetNow(Values v);

Values GetDisplayed();

void Print();

void Hide();
void Show();

} // namespace

#endif //POINTS_H__
