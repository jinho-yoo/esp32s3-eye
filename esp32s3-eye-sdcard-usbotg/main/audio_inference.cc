/**
 ******************************************************************************
 * @file    audio_inference.cpp
 * @brief   ACDNet 모델을 이용한 오디오(또는 기타) 데이터 추론 로직
 ******************************************************************************
 */

#include "audio_inference.h"
//#include "stm32h7xx_hal.h"
//#include "stm32_lcd.h"
#include "stdint.h"

#include "esp_timer.h"

#include "model_settings.h"  // kCategoryCount, kCategoryLabels 등
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "micro_speech_model.h" // acdnet_model_data (flatbuffer 배열)
#include "voting.h"             // get_top_prediction(), print_prediction(), print_time()

/* 익명 namespace: 전역 변수 스코프 제한 */
namespace {

/* 1) 모델 관련 객체 선언 */
const tflite::Model*      acdnet_model      = nullptr;
tflite::MicroInterpreter* acdnet_interpreter = nullptr;
TfLiteTensor*             acdnet_input       = nullptr;
TfLiteTensor*             acdnet_output      = nullptr;

/* 2) 모델 실행을 위한 메모리 풀 크기 및 버퍼 */
constexpr size_t kAcdnetArenaSize = 460 * 1024;  
alignas(8) uint8_t acdnet_arena[kAcdnetArenaSize] __attribute__((section(".ext_ram.bss")));

} // namespace

/**
 * @brief ACDNet 모델 및 인터프리터 초기화
 */
void audio_setup(void)
{
  // 1) MCU 타겟 초기화 (CMSIS, clock 등)
  tflite::InitializeTarget();

  // 2) FlatBuffer 모델 매핑
  acdnet_model = tflite::GetModel(acdnet_model_data);
  if (acdnet_model->version() != TFLITE_SCHEMA_VERSION)
  {
    //UTIL_LCD_DisplayStringAtLine(6, (uint8_t*)"[ACDNet] Model schema version mismatch!");
    return;
  }

  // 3) 필요한 연산자 등록 (op_resolver)
  static tflite::MicroMutableOpResolver<10> acdnet_op_resolver;
  acdnet_op_resolver.AddConv2D();
  acdnet_op_resolver.AddAveragePool2D();
  acdnet_op_resolver.AddQuantize();
  acdnet_op_resolver.AddDequantize();
  acdnet_op_resolver.AddMaxPool2D();
  acdnet_op_resolver.AddTranspose();
  acdnet_op_resolver.AddRelu();
  acdnet_op_resolver.AddReshape();
  acdnet_op_resolver.AddFullyConnected();
  acdnet_op_resolver.AddSoftmax();

  // 4) MicroInterpreter 인스턴스 생성
  static tflite::MicroInterpreter acd_interpreter(
      acdnet_model,
      acdnet_op_resolver,
      acdnet_arena,
      kAcdnetArenaSize
  );
  acdnet_interpreter = &acd_interpreter;

  // 5) Tensor 할당 (메모리 할당)
  TfLiteStatus allocate_status = acdnet_interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk)
  {
    //UTIL_LCD_DisplayStringAtLine(8, (uint8_t*)"[ACDNet] AllocateTensors() failed!");
    return;
  }

  // 6) 입출력 텐서 포인터 가져오기
  acdnet_input  = acdnet_interpreter->input(0);
  acdnet_output = acdnet_interpreter->output(0);


  // (필요시) LCD 출력 또는 디버깅 메시지
  //UTIL_LCD_DisplayStringAtLine(6, (uint8_t*)"[ACDNet] Model loaded & Tensors allocated.");
}

/**
 * @brief ACDNet 모델을 사용한 딥러닝 추론 함수
 * @param preprocessed_buffer 전처리된 int8 데이터(예: 30225 길이)
 */
int audio_inference(int8_t* preprocessed_buffer)
{
    int64_t start_time, end_time, elapsed_time_ms;
    if (acdnet_interpreter == nullptr || acdnet_input == nullptr || acdnet_output == nullptr)
    {
        //UTIL_LCD_DisplayStringAtLine(13, (uint8_t*)"[ACDNet] Interpreter not ready!");
        return -1;
    }


    const int data_len = 24000;
    for (int i = 0; i < data_len; ++i)
    {
        acdnet_input->data.int8[i] = preprocessed_buffer[i];
    }

    //uint32_t start_tick = HAL_GetTick();
    start_time = esp_timer_get_time();
    TfLiteStatus invoke_status = acdnet_interpreter->Invoke();
    end_time = esp_timer_get_time();
    elapsed_time_ms = (end_time - start_time) / 1000; // ms로 변환
    //uint32_t end_tick = HAL_GetTick();

    if (invoke_status != kTfLiteOk)
    {
        //UTIL_LCD_DisplayStringAtLine(13, (uint8_t*)"[ACDNet] Inference Invoke failed!");
        return -1;
    }


    const int num_categories = acdnet_output->dims->data[1];

    const int top_ind = get_top_prediction_float(acdnet_output->data.f, num_categories);

    print_prediction(kCategoryLabels[top_ind]);
    print_confidence(top_ind, 17);
    print_time(elapsed_time_ms, 19);

    return top_ind;
}
