/**
 ******************************************************************************
 * @file    voting.c
 * @brief   추론 결과 스코어 해석 및 LCD 표시
 ******************************************************************************
 */

#include "voting.h"
#include <stdio.h>
#include <string.h>
//#include "stm32_lcd.h"

static char lcd_output_string[64] = {0};

extern char text_buffer[1024];

/**
 * @brief 최대 점수를 가지는 인덱스(카테고리) 리턴
 * @param predictions  : (int8_t) 스코어 배열
 * @param num_categories : 카테고리 개수
 * @return 최대 점수를 갖는 카테고리 인덱스
 */
int get_top_prediction(const int8_t *predictions, int num_categories)
{
  int max_score = predictions[0];
  int guess_idx = 0;

  for (int i = 1; i < num_categories; i++)
  {
    if (predictions[i] > max_score)
    {
      max_score = predictions[i];
      guess_idx = i;
    }
  }
  return guess_idx;
}

int get_top_prediction_float(const float *predictions, int num_categories)
{
  float max_score = predictions[0];
  int   guess_idx = 0;
  for (int i = 1; i < num_categories; i++)
  {
    if (predictions[i] > max_score)
    {
      max_score = predictions[i];
      guess_idx = i;
    }
  }
  return guess_idx;
}

/**
 * @brief 카테고리 이름 LCD에 표시
 */
void print_prediction(const char *prediction)
{
  snprintf(lcd_output_string, sizeof(lcd_output_string), "  Prediction: %s\n", prediction);
  strcat(text_buffer,lcd_output_string);
  //UTIL_LCD_DisplayStringAtLine(7, (uint8_t*)lcd_output_string);
}

/**
 * @brief 추론 시간(ms)을 LCD에 표시
 * @param inference_time  : 추론 소요 시간(ms)
 * @param line            : LCD 출력 라인 번호
 */
void print_time(uint32_t inference_time, int8_t line)
{
  snprintf(lcd_output_string, sizeof(lcd_output_string), "  Inference time: %lu ms", inference_time);
  strcat(text_buffer,lcd_output_string);
  //UTIL_LCD_DisplayStringAtLine(line, (uint8_t*)lcd_output_string);
}

void print_confidence(int8_t max_score, int8_t line)
{
  // 텐서플로 라이트 int8 range는 [-128, 127]
  // 이를 0~255 범위로 매핑 후, 퍼센트로 변환
  double confidence_percent = ((double)(max_score + 128) / 255.0) * 100.0;

  snprintf(lcd_output_string, sizeof(lcd_output_string),
           "  Confidence: %.1f%%", confidence_percent);
  strcat(text_buffer,lcd_output_string);
 
  //UTIL_LCD_DisplayStringAtLine(line, (uint8_t*)lcd_output_string);
}
