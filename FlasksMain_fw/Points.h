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
#define VALUES_CNT      (HOUSES_CNT+1) // 4 houses and are_hidden

#define INDX_GRIF        0
#define INDX_SLYZE       1
#define INDX_RAVE        2
#define INDX_HUFF        3

namespace Points {

union Values {
    struct { int32_t grif, slyze, rave, huff, are_shown; };
    int32_t arr[VALUES_CNT]; // 4 houses and are_hidden
    int32_t Sum() { return grif + slyze + rave + huff; }
    int32_t Max() {
        int32_t         max = grif;
        if(slyze > max) max = slyze;
        if(rave > max)  max = rave;
        if(huff > max)  max = huff;
        return max;
    }
    Values() : grif(0), slyze(0), rave(0), huff(0), are_shown(1) {}
    Values(int16_t g, int16_t s, int16_t r, int16_t h, uint8_t shown) :
        grif(g), slyze(s), rave(r), huff(h), are_shown(shown) {}
};

void Init();
void Set(Values v);

Values GetDisplayed();

void Print();

void Hide();
void Show();

} // namespace

#endif //POINTS_H__
