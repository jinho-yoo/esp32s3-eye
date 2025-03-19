/* Copyright 2023 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef MODEL_SETTINGS_H_
#define MODEL_SETTINGS_H_

// The following values are derived from values used during model training.
// If you change the way you preprocess the input, update all these constants.
constexpr int kAudioSampleFrequency = 16000;
constexpr int kFeatureSize = 40; //number of frequency bin
constexpr int kFeatureCount = 49; //windows in 1second audio
constexpr int kFeatureElementCount = (kFeatureSize * kFeatureCount); //(1,1960)
constexpr int kFeatureStrideMs = 20;
constexpr int kFeatureDurationMs = 30;

// Variables for the model's output categories.
constexpr int kCategoryCount = 50;
//constexpr int kCategoryCount = 2;
//
//constexpr const char* kCategoryLabels[kCategoryCount] = {
//		"drone",
//		"gun"
//};
// ESC-50 공식 50개 클래스 라벨 (알파벳 순이 아닌, 원본 순서)
constexpr const char* kCategoryLabels[kCategoryCount] = {
    /*  1 */ "dog",
    /*  2 */ "rooster",
    /*  3 */ "pig",
    /*  4 */ "cow",
    /*  5 */ "frog",
    /*  6 */ "cat",
    /*  7 */ "hen",
    /*  8 */ "insects_flying",
    /*  9 */ "sheep",
    /* 10 */ "crow",
    /* 11 */ "rain",
    /* 12 */ "sea_waves",
    /* 13 */ "crackling_fire",
    /* 14 */ "crickets",
    /* 15 */ "chirping_birds",
    /* 16 */ "water_drops",
    /* 17 */ "wind",
    /* 18 */ "pouring_water",
    /* 19 */ "toilet_flush",
    /* 20 */ "thunderstorm",
    /* 21 */ "crying_baby",
    /* 22 */ "sneezing",
    /* 23 */ "coughing",
    /* 24 */ "footsteps",
    /* 25 */ "laughing",
    /* 26 */ "brushing_teeth",
    /* 27 */ "snoring",
    /* 28 */ "drinking_sipping",
    /* 29 */ "door_wood_knock",
    /* 30 */ "mouse_click",
    /* 31 */ "keyboard_typing",
    /* 32 */ "door_wood_creaks",
    /* 33 */ "can_opening",
    /* 34 */ "washing_machine",
    /* 35 */ "vacuum_cleaner",
    /* 36 */ "clock_alarm",
    /* 37 */ "clock_tick",
    /* 38 */ "glass_breaking",
    /* 39 */ "helicopter",
    /* 40 */ "chainsaw",
    /* 41 */ "siren",
    /* 42 */ "car_horn",
    /* 43 */ "engine",
    /* 44 */ "train",
    /* 45 */ "church_bells",
    /* 46 */ "airplane",
    /* 47 */ "fireworks",
    /* 48 */ "hand_saw",
    /* 49 */ "gun_shot",
    /* 50 */ "drilling"
};

#endif  // MODEL_SETTINGS_H_
