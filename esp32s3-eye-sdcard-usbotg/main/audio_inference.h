#ifndef __AUDIO_INFERENCE_H
#define __AUDIO_INFERENCE_H

#ifdef __cplusplus
extern "C"{
#endif

#include "stdint.h"

void audio_setup();
void audio_preprocess(int16_t* audio_buffer, int8_t* output_buffer);
int audio_inference(int8_t* preprocessed_buffer);
#ifdef __cplusplus
}
#endif

#endif
