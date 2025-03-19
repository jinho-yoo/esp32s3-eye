
set(src_tf_app_file "audio_inference.cc" "micro_speech_model.cc" "voting.c")
set(requires fatfs console)										
if(CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH)
    list(APPEND requires wear_levelling)
endif()

idf_component_register(SRCS "main.c" ${src_tf_app_file}
                    INCLUDE_DIRS "."
					REQUIRES "${requires}"
)
