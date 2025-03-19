#ifndef __VOTING_H
#define __VOTING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

int get_top_prediction(const int8_t* predictions, int num_categories);
int get_top_prediction_float(const float *predictions, int num_categories);


void print_prediction(const char* prediction);

void print_confidence(int8_t max_score, int8_t line);

void print_time(uint32_t inference_time, int8_t line);

#ifdef __cplusplus
}
#endif

#endif
