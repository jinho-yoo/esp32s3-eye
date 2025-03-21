#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>


#include "esp_log.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/esp32_s3_eye.h"
#include "bsp/display.h"
#include "lvgl.h"
#include <memory.h>

#include "format_wav.h"

#include "audio_inference.h"
#define TAG                "MIC_LCD"
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
// for i2s
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
#include "driver/i2s_pdm.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"

#define myUSBOTG_SDCARD


///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
// for sdcard
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

#ifdef myUSBOTG_SDCARD

#include <errno.h>
#include <dirent.h>
#include "esp_console.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"
#ifdef CONFIG_EXAMPLE_STORAGE_MEDIA_SDMMCCARD
#include "diskio_impl.h"
#include "diskio_sdmmc.h"
#endif

//static const char *TAG = "example_main";

/* TinyUSB descriptors
   ********************************************************************* */
#define EPNUM_MSC       1
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

enum {
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};

enum {
    EDPT_CTRL_OUT = 0x00,
    EDPT_CTRL_IN  = 0x80,

    EDPT_MSC_OUT  = 0x01,
    EDPT_MSC_IN   = 0x81,
};

static uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, TUD_OPT_HIGH_SPEED ? 512 : 64),
};

static tusb_desc_device_t descriptor_config = {
    .bLength = sizeof(descriptor_config),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A, // This is Espressif VID. This needs to be changed according to Users / Customers
    .idProduct = 0x4002,
    .bcdDevice = 0x100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

static char const *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },  // 0: is supported language is English (0x0409)
    "TinyUSB",                      // 1: Manufacturer
    "TinyUSB Device",               // 2: Product
    "123456",                       // 3: Serials
    "Example MSC",                  // 4. MSC
};
/*********************************************************************** TinyUSB descriptors*/

#define BASE_PATH "/data" // base path to mount the partition
#define SD_MOUNT_POINT "/data" // base path to mount the partition

#define PROMPT_STR CONFIG_IDF_TARGET
static int console_unmount(int argc, char **argv);
static int console_read(int argc, char **argv);
static int console_write(int argc, char **argv);
static int console_size(int argc, char **argv);
static int console_status(int argc, char **argv);
static int console_exit(int argc, char **argv);
const esp_console_cmd_t cmds[] = {
    {
        .command = "read",
        .help = "read BASE_PATH/README.MD and print its contents",
        .hint = NULL,
        .func = &console_read,
    },
    {
        .command = "write",
        .help = "create file BASE_PATH/README.MD if it does not exist",
        .hint = NULL,
        .func = &console_write,
    },
    {
        .command = "size",
        .help = "show storage size and sector size",
        .hint = NULL,
        .func = &console_size,
    },
    {
        .command = "expose",
        .help = "Expose Storage to Host",
        .hint = NULL,
        .func = &console_unmount,
    },
    {
        .command = "status",
        .help = "Status of storage exposure over USB",
        .hint = NULL,
        .func = &console_status,
    },
    {
        .command = "exit",
        .help = "exit from application",
        .hint = NULL,
        .func = &console_exit,
    }
};

// mount the partition and show all the files in BASE_PATH
static void _mount(void)
{
    ESP_LOGI(TAG, "Mount storage...");
    ESP_ERROR_CHECK(tinyusb_msc_storage_mount(BASE_PATH));

    
    // List all the files in this directory
    ESP_LOGI(TAG, "\nls command output:");
    struct dirent *d;
    DIR *dh = opendir(BASE_PATH);
    if (!dh) {
        if (errno == ENOENT) {
            //If the directory is not found
            ESP_LOGE(TAG, "Directory doesn't exist %s", BASE_PATH);
        } else {
            //If the directory is not readable then throw error and exit
            ESP_LOGE(TAG, "Unable to read directory %s", BASE_PATH);
        }
        return;
    }
    //While the next entry is not readable we will print directory files
    while ((d = readdir(dh)) != NULL) {
        printf("%s\n", d->d_name);
    }
    return;
}

// unmount storage
static int console_unmount(int argc, char **argv)
{
    if (tinyusb_msc_storage_in_use_by_usb_host()) {
        ESP_LOGE(TAG, "storage is already exposed");
        return -1;
    }
    ESP_LOGI(TAG, "Unmount storage...");
    ESP_ERROR_CHECK(tinyusb_msc_storage_unmount());
    return 0;
}

// read BASE_PATH/README.MD and print its contents
static int console_read(int argc, char **argv)
{
    if (tinyusb_msc_storage_in_use_by_usb_host()) {
        ESP_LOGE(TAG, "storage exposed over USB. Application can't read from storage.");
        return -1;
    }
    ESP_LOGD(TAG, "read from storage:");
    const char *filename = BASE_PATH "/README.MD";
    FILE *ptr = fopen(filename, "r");
    if (ptr == NULL) {
        ESP_LOGE(TAG, "Filename not present - %s", filename);
        return -1;
    }
    char buf[1024];
    while (fgets(buf, 1000, ptr) != NULL) {
        printf("%s", buf);
    }
    fclose(ptr);
    return 0;
}

// create file BASE_PATH/README.MD if it does not exist
static int console_write(int argc, char **argv)
{
    if (tinyusb_msc_storage_in_use_by_usb_host()) {
        ESP_LOGE(TAG, "storage exposed over USB. Application can't write to storage.");
        return -1;
    }
    ESP_LOGD(TAG, "write to storage:");
    const char *filename = BASE_PATH "/README.MD";
    FILE *fd = fopen(filename, "r");
    if (!fd) {
        ESP_LOGW(TAG, "README.MD doesn't exist yet, creating");
        fd = fopen(filename, "w");
        fprintf(fd, "Mass Storage Devices are one of the most common USB devices. It use Mass Storage Class (MSC) that allow access to their internal data storage.\n");
        fprintf(fd, "In this example, ESP chip will be recognised by host (PC) as Mass Storage Device.\n");
        fprintf(fd, "Upon connection to USB host (PC), the example application will initialize the storage module and then the storage will be seen as removable device on PC.\n");
        fclose(fd);
    }
    return 0;
}

// Show storage size and sector size
static int console_size(int argc, char **argv)
{
    if (tinyusb_msc_storage_in_use_by_usb_host()) {
        ESP_LOGE(TAG, "storage exposed over USB. Application can't access storage");
        return -1;
    }
    uint32_t sec_count = tinyusb_msc_storage_get_sector_count();
    uint32_t sec_size = tinyusb_msc_storage_get_sector_size();
    printf("Storage Capacity %lluMB\n", ((uint64_t) sec_count) * sec_size / (1024 * 1024));
    return 0;
}

// exit from application
static int console_status(int argc, char **argv)
{
    printf("storage exposed over USB: %s\n", tinyusb_msc_storage_in_use_by_usb_host() ? "Yes" : "No");
    return 0;
}

// exit from application
static int console_exit(int argc, char **argv)
{
    tinyusb_msc_storage_deinit();
    printf("Application Exiting\n");
    exit(0);
    return 0;
}

#ifdef CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH
static esp_err_t storage_init_spiflash(wl_handle_t *wl_handle)
{
    ESP_LOGI(TAG, "Initializing wear levelling");

    const esp_partition_t *data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL);
    if (data_partition == NULL) {
        ESP_LOGE(TAG, "Failed to find FATFS partition. Check the partition table.");
        return ESP_ERR_NOT_FOUND;
    }

    return wl_mount(data_partition, wl_handle);
}
#else  // CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH
//sdmmc_host_t host= SDMMC_HOST_DEFAULT();
static esp_err_t storage_init_sdmmc(sdmmc_card_t **card)
{
    esp_err_t ret = ESP_OK;
    bool host_init = false;
    sdmmc_card_t *sd_card;

    ESP_LOGI(TAG, "Initializing SDCard");

    //gpio_set_pull_mode(38, GPIO_PULLUP_ONLY); //yoojh: internal pull-ups note!!!: too high resistor 50 kHom(ideal:10kohm)
    //gpio_set_pull_mode(39, GPIO_PULLUP_ONLY); //yoojh: internal pull-ups
    //gpio_set_pull_mode(40, GPIO_PULLUP_ONLY); //yoojh: internal pull-ups

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 40MHz for SDMMC)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();    

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // For SD Card, set bus width to use
#ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
    slot_config.width = 4;
#else
    slot_config.width = 1;
#endif  // CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4

    // On chips where the GPIOs used for SD card can be configured, set the user defined values
#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
    slot_config.clk = CONFIG_EXAMPLE_PIN_CLK;
    slot_config.cmd = CONFIG_EXAMPLE_PIN_CMD;
    slot_config.d0 = CONFIG_EXAMPLE_PIN_D0;
#ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
    slot_config.d1 = CONFIG_EXAMPLE_PIN_D1;
    slot_config.d2 = CONFIG_EXAMPLE_PIN_D2;
    slot_config.d3 = CONFIG_EXAMPLE_PIN_D3;
#endif  // CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4

#endif  // CONFIG_SOC_SDMMC_USE_GPIO_MATRIX

    // Enable internal pullups on enabled pins. The internal pullups
    // are insufficient however, please make sure 10k external pullups are
    // connected on the bus. This is for debug / example purpose only.
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    // not using ff_memalloc here, as allocation in internal RAM is preferred
    sd_card = (sdmmc_card_t *)malloc(sizeof(sdmmc_card_t));
    ESP_GOTO_ON_FALSE(sd_card, ESP_ERR_NO_MEM, clean, TAG, "could not allocate new sdmmc_card_t");

    ESP_GOTO_ON_ERROR((*host.init)(), clean, TAG, "Host Config Init fail");
    host_init = true;

    ESP_GOTO_ON_ERROR(sdmmc_host_init_slot(host.slot, (const sdmmc_slot_config_t *) &slot_config),
                      clean, TAG, "Host init slot fail");

    while (sdmmc_card_init(&host, sd_card)) {
        ESP_LOGE(TAG, "The detection pin of the slot is disconnected(Insert uSD card). Retrying...");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, sd_card);
    *card = sd_card;

    return ESP_OK;

clean:
    if (host_init) {
        if (host.flags & SDMMC_HOST_FLAG_DEINIT_ARG) {
            host.deinit_p(host.slot);
        } else {
            (*host.deinit)();
        }
    }
    if (sd_card) {
        free(sd_card);
        sd_card = NULL;
    }
    return ret;
}
#endif  // CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH


int first=0;
//#define TESTSIZE 24000
#define TESTSIZE 20
//uint8_t testdata[TESTSIZE];
uint8_t testdata[TESTSIZE] __attribute__((section(".ext_ram.bss")));;
void test_file(uint8_t ch)
{
    if (first==0) {    
    const char *filename = BASE_PATH "/filecreate-total0.txt";
    FILE *fd = fopen(filename, "w");
    fprintf(fd, "first=%d,rage Devices file creation test.\n", first);
    for (int kk=0;kk<TESTSIZE;kk++) {
        //fprintf(fd, "0x%02x, ", kk);
        testdata[kk]='0'+(kk%10);
        if (testdata[kk]=='0') testdata[kk]='\n';
        //if (kk%10==0) fprintf(fd, "\n");
        //fflush(fd);
    }
    //fwrite(testdata, TESTSIZE, 1, fd);
    
    fclose(fd);
    first++;
    } else
    if (first==1) {
        const char *filename = BASE_PATH "/filecreate-total2.txt";
        FILE *fd = fopen(filename, "w");
        fprintf(fd, "first=%d,Mass Storage Devices file creation test.\n",first);
        fclose(fd);
        first++;
    } else
    if (first==2) {
            const char *filename = BASE_PATH "/filecreate-total2.txt";
            FILE *fd = fopen(filename, "w");
            fprintf(fd, "1Mass Storage Devices file creation test.0x%02x\n", ch);
            fclose(fd);
            first++;
    }    
}

volatile bool sd_started=false;

void usbotg_sdcard()
{
//yoojh(SDCARD USB) (begin)
ESP_LOGI(TAG, "Initializing storage...");
	
#ifdef CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH
    static wl_handle_t wl_handle = WL_INVALID_HANDLE;
    ESP_ERROR_CHECK(storage_init_spiflash(&wl_handle));

    const tinyusb_msc_spiflash_config_t config_spi = {
        .wl_handle = wl_handle
    };
    ESP_ERROR_CHECK(tinyusb_msc_storage_init_spiflash(&config_spi));
#else // CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH
    static sdmmc_card_t *card = NULL;
    
    ESP_ERROR_CHECK(storage_init_sdmmc(&card));

    const tinyusb_msc_sdmmc_config_t config_sdmmc = {
        .card = card
    };
    ESP_ERROR_CHECK(tinyusb_msc_storage_init_sdmmc(&config_sdmmc));

#endif  // CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH

    //mounted in the app by default
    _mount();

    //test_file('a');

    ESP_LOGI(TAG, "USB MSC initialization");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &descriptor_config,
        .string_descriptor = string_desc_arr,
        .string_descriptor_count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]),
        .external_phy = false,
        .configuration_descriptor = desc_configuration,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB MSC initialization DONE");

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
 
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */

    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = 64;
    esp_console_register_help_command();
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
    for (int count = 0; count < sizeof(cmds) / sizeof(esp_console_cmd_t); count++) {
        ESP_ERROR_CHECK( esp_console_cmd_register(&cmds[count]) );
    }
    ESP_ERROR_CHECK(esp_console_start_repl(repl));

sd_started=true;    
//yoojh(SDCARD USB) (end)  
}

bool is_started_usbotg_sdcard()
{
    return sd_started;
}

void sdcard_only_init(void) {
    esp_err_t ret;

    // SDMMC 호스트 설정
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;  // 1비트 모드 설정

    // SDMMC 슬롯 설정 (1비트 모드)
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;  // 1비트 모드 지정

    // On chips where the GPIOs used for SD card can be configured, set the user defined values
#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
    slot_config.clk = CONFIG_EXAMPLE_PIN_CLK;
    slot_config.cmd = CONFIG_EXAMPLE_PIN_CMD;
    slot_config.d0 = CONFIG_EXAMPLE_PIN_D0;
#ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
    slot_config.d1 = CONFIG_EXAMPLE_PIN_D1;
    slot_config.d2 = CONFIG_EXAMPLE_PIN_D2;
    slot_config.d3 = CONFIG_EXAMPLE_PIN_D3;
#endif  // CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
#endif
    // Enable internal pullups on enabled pins. The internal pullups
    // are insufficient however, please make sure 10k external pullups are
    // connected on the bus. This is for debug / example purpose only.
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
	
	
    // FAT 파일 시스템 마운트 설정
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,  // 마운트 실패 시 포맷 여부
        .max_files = 5,                   // 최대 열 수 있는 파일 수
        .allocation_unit_size = 16 * 1024  // 클러스터 크기
    };

    sdmmc_card_t *card;
    ret = esp_vfs_fat_sdmmc_mount(BASE_PATH, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        printf("Failed to mount SD card: %s\n", esp_err_to_name(ret));
        return;
    }

    printf("SD card mounted successfully in 1-bit mode!\n");

    // 파일 생성 및 쓰기
    FILE *f = fopen(BASE_PATH"/test.txt", "w");
    if (f) {
        for (int p=0;p<24000;p++)
        fprintf(f, "%d Hello from ESP32-S3 using SDMMC 1-bit mode FAT32!\n", p);
        fclose(f);
        printf("File written successfully\n");
    } else {
        printf("Failed to open file for writing\n");
    }

    test_file('a');
    // SD 카드 언마운트 (필요한 경우)
    esp_vfs_fat_sdcard_unmount(BASE_PATH, card);
}



int faudio=0;
void test_fileaudio(uint8_t ch)
{
    FILE *fd;
    const char *filename = BASE_PATH "/audio1.txt";
    if (faudio==0) {
      fd = fopen(filename, "w");
      faudio=1;
    } else {
      fd = fopen(filename, "a");
    }
    fprintf(fd, "0Mass Storage Devices file creation test.\n");
    for (int kk=0;kk<TESTSIZE;kk++) {
        //fprintf(fd, "0x%02x, ", kk);
        testdata[kk]='0'+(kk%10);
        if (testdata[kk]=='0') testdata[kk]='\n';
        //if (kk%10==0) fprintf(fd, "\n");
        //fflush(fd);
    }
    fwrite(testdata, TESTSIZE, 1, fd);
    
    fclose(fd);
    
}

#endif

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
// for sdcard (end)
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
// this is hard coded yoojh(begin)
#define NUM_CHANNELS        (1) // For mono recording only!
#define CONFIG_EXAMPLE_SAMPLE_RATE 44100 // refer  i2s_recorder_main.c, Kconfig.probuild
#define CONFIG_EXAMPLE_BIT_SAMPLE 16 // refer  i2s_recorder_main.c, Kconfig.probuild
#define SAMPLE_SIZE         (CONFIG_EXAMPLE_BIT_SAMPLE * 1024)
#define BYTE_RATE           (CONFIG_EXAMPLE_SAMPLE_RATE * (CONFIG_EXAMPLE_BIT_SAMPLE / 8)) * NUM_CHANNELS
// this is hard coded yoojh(end)

#define SAMPLE_RATE        16000       // 샘플 레이트 (16kHz)
#define PCM_BUFFER_SIZE    1024        // PCM 버퍼 크기
#define I2S_NUM            I2S_NUM_1   // I2S 포트


#define AUDIO_SIZE 24000
#define TEXT_BUF_SIZE 256

int8_t audio_buffer[AUDIO_SIZE] __attribute__((section(".ext_ram.bss")));;
char text_buffer[TEXT_BUF_SIZE];

int count=0;

int fill_pcm = 0;
int fill_cnt = 0;

static i2s_chan_handle_t rx_handle = NULL;
lv_obj_t *label;  // LCD에 텍스트를 표시할 레이블 객체


// I2S 초기화 함수
static esp_err_t i2s_init(void) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_8BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = GPIO_NUM_NC,     // Master clock (사용하지 않음)
            .bclk = GPIO_NUM_41,     // Bit clock
            .ws   = GPIO_NUM_42,     // Word select
            .din  = GPIO_NUM_2,      // Data input
            .dout = GPIO_NUM_NC,     // Data output (사용하지 않음)
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
    return ESP_OK;
}

// LVGL 태스크 함수
static void lvgl_task(void *arg) {
    while (1) {
        bsp_display_lock(0);  // LVGL Mutex 잠금
        lv_timer_handler();   // LVGL 이벤트 처리
        bsp_display_unlock(); // LVGL Mutex 해제
        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms 대기
    }
}

int mycc=0;
FILE *gfd;

// PCM 데이터 읽기 및 LCD 출력
static void read_pcm_data(void *arg) {

#ifdef myUSBOTG_SDCARD
    //while (!is_started_usbotg_sdcard()) ;
#endif

    int8_t *pcm_buffer = (int8_t *)malloc(PCM_BUFFER_SIZE * sizeof(int8_t));
    if (!pcm_buffer) {
        ESP_LOGE(TAG, "Failed to allocate PCM buffer");
        vTaskDelete(NULL);
        return;
    }

   /* audio_buffer = (int8_t *)malloc(AUDIO_SIZE);

    if (!audio_buffer) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        vTaskDelete(NULL);
        return;
    }*/
/*
    // Use POSIX and C standard library functions to work with files.
    int flash_wr_size = 0;
    ESP_LOGI(TAG, "Opening file");    

        // First check if file exists before creating a new file.
        struct stat st;
        if (stat(SD_MOUNT_POINT"/record.wav", &st) == 0) {
            // Delete it if it exists
            unlink(SD_MOUNT_POINT"/record.wav");
        }
    
        // Create new WAV file
        FILE *f = fopen(SD_MOUNT_POINT"/record.wav", "a");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for writing");
            return;
        }
    
        // Write the header to the WAV file
        fwrite(&wav_header, sizeof(wav_header), 1, f);
*/
/*
if (count==0) {
// save wav file(begin)
uint32_t rec_time=15; // 15 sec hard coding later will be fixed yoojh
uint32_t flash_rec_time = BYTE_RATE * rec_time;
const wav_header_t wav_header =
//WAV_HEADER_PCM_DEFAULT(flash_rec_time, 16, CONFIG_EXAMPLE_SAMPLE_RATE, 1); //SAMPLE_RATE
WAV_HEADER_PCM_DEFAULT(flash_rec_time, 16, SAMPLE_RATE, 1);

// First check if file exists before creating a new file.
struct stat st;
const char *fn1 = BASE_PATH "/record.wav";
if (stat(fn1, &st) == 0) {
// Delete it if it exists
unlink(fn1);
}
} // if (count==0)

// Create new WAV file
FILE *f = fopen(SD_MOUNT_POINT "/record.wav", "wb+");
if (f == NULL) {
ESP_LOGE(TAG, "Failed to open file for writing");
return;
}

//// Write the header to the WAV file
fprintf(f, "%d) start.\n", count++);
*/
// save wav file(begin)

    while (true) {
        size_t bytes_read = 0;

        // PCM 데이터를 읽어서 버퍼에 저장
        esp_err_t ret = i2s_channel_read(rx_handle, pcm_buffer, PCM_BUFFER_SIZE * sizeof(int8_t), &bytes_read, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read PCM data: %s", esp_err_to_name(ret));
            continue;
        }
        //fwrite(pcm_buffer, bytes_read, 1, f);
        /*
        if (count<200) fprintf(f, "%d) start.%d\n", count++, bytes_read);
        if (count==200) fclose(f);
        */
      
        if (!fill_pcm && bytes_read)
        {
            for (size_t i = 0; i < bytes_read ; i++)
            {
                if (fill_cnt < AUDIO_SIZE)
                    audio_buffer[fill_cnt] = pcm_buffer[2 * i];//(int8_t)((pcm_buffer[2 * i] - 32768) / 256);
                else
                    break;

                fill_cnt++;
            }
        }

        if (fill_cnt == AUDIO_SIZE)
        {
            fill_pcm = 1;
            fill_cnt = 0;
        }

 
 
        ESP_LOGI(TAG, "PCM: %d %d %d %d %d %d %d %d %d %d %d %d", pcm_buffer[0], pcm_buffer[1], pcm_buffer[2], pcm_buffer[3], pcm_buffer[4], pcm_buffer[5]
        , pcm_buffer[6], pcm_buffer[7], pcm_buffer[8], pcm_buffer[9], pcm_buffer[10], pcm_buffer[11]);  // 로그로 출력
#if 0
//#ifdef myUSBOTG_SDCARD
        if (mycc<10) test_file(pcm_buffer[0]); //write((uint8_t)pcm_buffer[0]);

        char *filename = BASE_PATH "/audiof.wav";        
        FILE *gfd = fopen(filename, "a");
        for (int k=0;k<10;k++) {
          fprintf(gfd, "%d]0x%02x,", k,pcm_buffer[k]);
        }
        fprintf(gfd, "\n");
        fclose(gfd);
    
        mycc++;
#endif  
        // // CPU 사용량 감소를 위해 대기
    }
    //fclose(f);
    free(pcm_buffer);
    vTaskDelete(NULL);
}

#if 0
// direct save(begin)

#define NUM_CHANNELS        (1) // For mono recording only!

#define SAMPLE_SIZE         (CONFIG_EXAMPLE_BIT_SAMPLE * 1024)
#define BYTE_RATE           (CONFIG_EXAMPLE_SAMPLE_RATE * (CONFIG_EXAMPLE_BIT_SAMPLE / 8)) * NUM_CHANNELS
#define CONFIG_EXAMPLE_SAMPLE_RATE 44100
#define CONFIG_EXAMPLE_REC_TIME 15


size_t bytes_read;
static int16_t i2s_readraw_buff[SAMPLE_SIZE];

void record_wav(uint32_t rec_time)
{
    // Use POSIX and C standard library functions to work with files.
    int flash_wr_size = 0;
    ESP_LOGI(TAG, "Opening file");

    uint32_t flash_rec_time = BYTE_RATE * rec_time;
    const wav_header_t wav_header =
        WAV_HEADER_PCM_DEFAULT(flash_rec_time, 16, CONFIG_EXAMPLE_SAMPLE_RATE, 1);

    // First check if file exists before creating a new file.
    struct stat st;
    if (stat(BASE_PATH"/record.wav", &st) == 0) {
        // Delete it if it exists
        unlink(BASE_PATH"/record.wav");
    }

    // Create new WAV file
    //FILE *f = fopen(BASE_PATH"/record.wav", "a");
    FILE *f = fopen(BASE_PATH"/record.wav", "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    // Write the header to the WAV file
    fwrite(&wav_header, sizeof(wav_header), 1, f);

    // Start recording
    while (flash_wr_size < flash_rec_time) {
        // Read the RAW samples from the microphone
        if (i2s_channel_read(rx_handle, (char *)i2s_readraw_buff, SAMPLE_SIZE, &bytes_read, 1000) == ESP_OK) {
            printf("[0] %d [1] %d [2] %d [3]%d ...\n", i2s_readraw_buff[0], i2s_readraw_buff[1], i2s_readraw_buff[2], i2s_readraw_buff[3]);
            // Write the samples to the WAV file
            fwrite(i2s_readraw_buff, bytes_read, 1, f);
            flash_wr_size += bytes_read;
        } else {
            printf("Read Failed!\n");
        }
    }

    ESP_LOGI(TAG, "Recording done!");
    fclose(f);
    ESP_LOGI(TAG, "File written on SDCard");

    // All done, unmount partition and disable SPI peripheral
    //yoojh esp_vfs_fat_sdcard_unmount(BASE_PATH, card);
    ESP_LOGI(TAG, "Card unmounted");
    // Deinitialize the bus after all devices are removed
    //yoojh spi_bus_free(host.slot); yoojh check this as comment later
}

// direct save(end)
#endif

#if 0
void app_main(void) {

    ESP_LOGI("MEMORY", "Free internal heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI("MEMORY", "Free PSRAM heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  
	ESP_LOGI(TAG, "Initializing I2S...");
    if (i2s_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2S initialization failed");
        return;
    }

    ESP_LOGI(TAG, "Initializing BSP...");
    //yoojh
#if 0    
    lv_display_t *disp = bsp_display_start();
    if (!disp) {
        ESP_LOGE(TAG, "Failed to initialize display.");
        return;
    }

    // 백라이트 설정
    ESP_ERROR_CHECK(bsp_display_brightness_set(100));

    //yoojh 1->0
#if 1
    // LVGL 화면 초기화
    bsp_display_lock(0);  // LVGL Mutex 잠금

    lv_obj_t *scr = lv_obj_create(NULL);  // 새 화면 생성
    lv_scr_load(scr);                     // 새 화면 활성화

    label = lv_label_create(scr);  // 레이블 생성
    lv_obj_set_width(label, 200); // Label 너비 설정
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, "yoojh_OK-PCM: Initializing...");  // 초기 텍스트 설정
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);       // 화면 중앙에 배치

    bsp_display_unlock();  // LVGL Mutex 해제
#endif
#endif
    ESP_LOGI(TAG, "Starting tasks...");
    //yoojh audio_setup();

#ifdef myUSBOTG_SDCARD
    usbotg_sdcard();
    test_file('a');
#endif
#if 0
#ifdef myUSBOTG_SDCARD
        char *filename = BASE_PATH "/audiof.wav";        
        FILE *gfd = fopen(filename, "a");
#endif  
#endif

record_wav(CONFIG_EXAMPLE_REC_TIME);

#if 0
    // LVGL 태스크 실행
    xTaskCreatePinnedToCore(lvgl_task, "LVGL_Task", 4 * 1024, NULL, 1, NULL, 1);

    // PCM 데이터 읽기 태스크 실행
    xTaskCreatePinnedToCore(read_pcm_data, "read_pcm_data", 4 * 1024, NULL, 5, NULL, 0);



    while (1) {
        if (fill_pcm)
        {
            memset(text_buffer,0,TEXT_BUF_SIZE);
            int top_ind = audio_inference(audio_buffer);
            //test_filewrite();


            bsp_display_lock(0);  // LVGL Mutex 잠금
            lv_label_set_text(label, text_buffer);  // 레이블 텍스트 업데이트
            bsp_display_unlock();  // LVGL Mutex 해제

            fill_pcm = 0;
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

    }

#ifdef myUSBOTG_SDCARD
        fclose(gfd);
#endif  
#endif
}



void app_main(void) {

    ESP_LOGI("MEMORY", "Free internal heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI("MEMORY", "Free PSRAM heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  
	ESP_LOGI(TAG, "Initializing I2S...");
    if (i2s_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2S initialization failed");
        return;
    }

    ESP_LOGI(TAG, "Initializing BSP...");
    lv_display_t *disp = bsp_display_start();
    if (!disp) {
        ESP_LOGE(TAG, "Failed to initialize display.");
        return;
    }

    // 백라이트 설정
    ESP_ERROR_CHECK(bsp_display_brightness_set(100));

#if 1
    // LVGL 화면 초기화
    bsp_display_lock(0);  // LVGL Mutex 잠금

    lv_obj_t *scr = lv_obj_create(NULL);  // 새 화면 생성
    lv_scr_load(scr);                     // 새 화면 활성화

    label = lv_label_create(scr);  // 레이블 생성
    lv_obj_set_width(label, 200); // Label 너비 설정
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, "yoojh_OK-PCM: Initializing...");  // 초기 텍스트 설정
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);       // 화면 중앙에 배치

    bsp_display_unlock();  // LVGL Mutex 해제
#endif
    ESP_LOGI(TAG, "Starting tasks...");
    audio_setup();

#ifdef myUSBOTG_SDCARD
    usbotg_sdcard();
    test_file('a');
#endif

#ifdef myUSBOTG_SDCARD
        char *filename = BASE_PATH "/audiof.wav";        
        FILE *gfd = fopen(filename, "a");
#endif  

    // LVGL 태스크 실행
    xTaskCreatePinnedToCore(lvgl_task, "LVGL_Task", 4 * 1024, NULL, 1, NULL, 1);

    // PCM 데이터 읽기 태스크 실행
    xTaskCreatePinnedToCore(read_pcm_data, "read_pcm_data", 4 * 1024, NULL, 5, NULL, 0);

    while (1) {
        if (fill_pcm)
        {
            memset(text_buffer,0,TEXT_BUF_SIZE);
            int top_ind = audio_inference(audio_buffer);
            //test_filewrite();


            bsp_display_lock(0);  // LVGL Mutex 잠금
            lv_label_set_text(label, text_buffer);  // 레이블 텍스트 업데이트
            bsp_display_unlock();  // LVGL Mutex 해제

            fill_pcm = 0;
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

    }

#ifdef myUSBOTG_SDCARD
        fclose(gfd);
#endif  

}
#endif



void app_main(void) {

    ESP_LOGI("MEMORY", "Free internal heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI("MEMORY", "Free PSRAM heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  
	ESP_LOGI(TAG, "Initializing I2S...");
    if (i2s_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2S initialization failed");
        return;
    }
//disable DISPLAY    
#if 1
    ESP_LOGI(TAG, "Initializing BSP...");
    lv_display_t *disp = bsp_display_start();
    if (!disp) {
        ESP_LOGE(TAG, "Failed to initialize display.");
        return;
    }

    // 백라이트 설정
    ESP_ERROR_CHECK(bsp_display_brightness_set(100));

#if 1
    // LVGL 화면 초기화
    bsp_display_lock(0);  // LVGL Mutex 잠금

    lv_obj_t *scr = lv_obj_create(NULL);  // 새 화면 생성
    lv_scr_load(scr);                     // 새 화면 활성화

    label = lv_label_create(scr);  // 레이블 생성
    lv_obj_set_width(label, 200); // Label 너비 설정
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, "yoojh_OK-PCM: Initializing...");  // 초기 텍스트 설정
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);       // 화면 중앙에 배치

    bsp_display_unlock();  // LVGL Mutex 해제
#endif
#endif //disable DISPLAY   

    ESP_LOGI(TAG, "Starting tasks...");
    audio_setup();

#ifdef myUSBOTG_SDCARD
    sdcard_only_init();
    
    

    usbotg_sdcard();
#endif

//disable DISPLAY    
#if 1
    // LVGL 태스크 실행
    xTaskCreatePinnedToCore(lvgl_task, "LVGL_Task", 4 * 1024, NULL, 1, NULL, 1);
#endif //disable DISPLAY

    // PCM 데이터 읽기 태스크 실행
    xTaskCreatePinnedToCore(read_pcm_data, "read_pcm_data", 4 * 1024, NULL, 5, NULL, 0);
int count=0;
//test_fileaudio('b');


    while (1) {
        if (fill_pcm)
        {
//disable DISPLAY    
          
            memset(text_buffer,0,TEXT_BUF_SIZE);

            int top_ind = audio_inference(audio_buffer);


            bsp_display_lock(0);  // LVGL Mutex 잠금
            lv_label_set_text(label, text_buffer);  // 레이블 텍스트 업데이트
            bsp_display_unlock();  // LVGL Mutex 해제

            //if (count++<1) test_fileaudio('b');

            fill_pcm = 0;
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

    }

  

}