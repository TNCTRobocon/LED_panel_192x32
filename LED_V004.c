// コマンド
//  gcc -o LED_V004 LED_V004.c -lwiringPi -lpthread -g -Wall

// ラズパイ実行時はコメントアウト解除
#define pi

#ifndef pi

#define _CRT_SECURE_NO_WARNINGS
#define HAVE_STRUCT_TIMESPEC

#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#ifndef pi

#else

#include <wiringPi.h>
#include <unistd.h>

#endif

#define NUM_THREAD 3 // スレッドの数

// パネルのサイズ定義
const int display_width = 192;
const int display_height = 32;

// ピン定義
const int G1 = 8;
const int G2 = 9;
const int R1 = 7;
const int R2 = 2;
const int B1 = 0;
const int B2 = 3;
const int A = 15;
const int B = 16;
const int C = 1;
const int D = 4;
const int CLK = 5;
const int LAT = 13;
const int OE = 12;

// PWMパターン
const unsigned long long pattern[16] = {
    0x00000000,                                                         /*0*/
    0x00000000,                                                         /*1*/
    0x00000000,                                                         /*2*/
    0x00000000,                                                         /*3*/
    0b1101101101101101101101101101101101101101101101101101101101101101, /*4*/
    0b1101101101101101101101101101101101101101101101101101101101101101, /*5*/
    0b1101101101101101101101101101101101101101101101101101101101101101, /*6*/
    0b1101101101101101101101101101101101101101101101101101101101101101, /*7*/
    0b1101101101101101101101101101101101101101101101101101101101101101, /*8*/
    0b1101101101101101101101101101101101101101101101101101101101101101, /*9*/
    0b1101101101101101101101101101101101101101101101101101101101101101, /*10*/
    0b1101101101101101101101101101101101101101101101101101101101101101, /*11*/
    0xffffffffffffffff,                                                 /*12*/
    0xffffffffffffffff,                                                 /*13*/
    0xffffffffffffffff,                                                 /*14*/
    0xffffffffffffffff,                                                 /*15*/
};

// エラーコード
const int malloc_NULL = -10;
const int calloc_NULL = -11;
const int realloc_NULL = -12;
const int file_NULL = -20;

unsigned int debug = 0;

// ファイルからのデータを入れる用
typedef struct
{
    size_t size;
    int seets;
    int *width;
    int *height;
    unsigned char *file_data;
} image;

// LEDパネル用レジスタ
typedef struct
{
    unsigned char *R;
    unsigned char *G;
    unsigned char *B;
} LED_panel;

typedef struct
{
    int finish_1A;
    int finish_1B;
    int finish_2A;
    int finish_2B;
    int startA;
    int startB;
    int x;
    int y;
    int count; // PWM制御用
} finish;

// LEDパネルレジスタ宣言
LED_panel LED_data;

// マルチスレッド同期用
finish multi_finish;

#ifndef pi
// 疑似wiringPi関連
#define HIGH 1
#define LOW 0
#define INPUT 1
#define OUTPUT 0

typedef struct
{
    int LED_io;
    int LED_output;
} LED;

LED LED_status[30] = {0};

void pinMode(int pin, int status);
void digitalWrite(int pin, int status);
void delay(size_t);
void delayMicroseconds(size_t);
void wiringPiSetup(void);

#endif

void file_load(image *);
void file_decode(image *, unsigned char *);
void calloc_check_int(int **, size_t);
void calloc_check_uchar(unsigned char **, size_t);
void display_image_write(unsigned char *, unsigned int, unsigned int, size_t);
void datalook(unsigned char *, size_t, size_t);
void print(void);
void *LED_drive_A(void *);
void pin_setup(void);
void ALLpin_LOW(void);

int main(void)
{
    image maindata = {0};             // ファイルから読みこんだデータ
    unsigned char *decodedata = NULL; // デコードしたデータを入れる
    pthread_t t[NUM_THREAD];

    file_load(&maindata); // ファイルを読みこんでmaindataに格納

    calloc_check_uchar(&decodedata, sizeof(unsigned char) * display_width * display_height * 2); // デコードしたデータ用のメモリ確保
    calloc_check_uchar(&LED_data.R, sizeof(unsigned char) * display_width * display_height);     // LEDパネルレジスタ用のメモリ確保
    calloc_check_uchar(&LED_data.G, sizeof(unsigned char) * display_width * display_height);     // LEDパネルレジスタ用のメモリ確保
    calloc_check_uchar(&LED_data.B, sizeof(unsigned char) * display_width * display_height);     // LEDパネルレジスタ用のメモリ確保


    file_decode(&maindata, decodedata);

    pin_setup();
    ALLpin_LOW();

    display_image_write(decodedata, 0, 0, display_width * display_height);

    pthread_create(&t[0], NULL, LED_drive_A, NULL);

    while (1)
    {
    }
}

void file_load(image *maindata)
{
    // ファイルを読み込んでmaindataに格納
    FILE *fp = NULL;
    unsigned int file_ver = 0; // 使わないけど必要なので渋々定義
    unsigned int space = 0;    // 空きスペース読み込み用

    // ファイルオープン
    fp = fopen("data.dat", "rb");
    if (fp == NULL)
    {
        printf("FILE NULL");
        exit(-20);
    }

    // ファイル読み込み(ファイル形式データ)
    fread(&file_ver, sizeof(unsigned int), 1, fp);
    fread(&maindata->size, sizeof(size_t), 1, fp);
    fread(&maindata->seets, sizeof(unsigned int), 1, fp);
    for (int i = 0; i < 7; i++)
    {
        fread(&space, sizeof(unsigned int), 1, fp);
    }

    // メインデータ用メモリ確保
    calloc_check_uchar(&maindata->file_data, maindata->size);
    calloc_check_int(&maindata->width, sizeof(unsigned int) * maindata->seets);
    calloc_check_int(&maindata->height, sizeof(unsigned int) * maindata->seets);

    // ファイル読み込み(ファイル形式データ)
    fread(maindata->file_data, sizeof(unsigned char), maindata->size, fp);
    fread(maindata->width, sizeof(unsigned int), maindata->seets, fp);
    fread(maindata->height, sizeof(unsigned int), maindata->seets, fp);

    // datalook(maindata->file_data ,maindata->size ,(size_t)maindata->width );

    fclose(fp);
}

void file_decode(image *maindata, unsigned char *display_data)
{
    // ファイルデコード(filedata>data)
    // printf("%d\n", maindata->width[0]);
    // printf("%d\n", maindata->height[0]);

    // datalook(maindata->file_data ,*maindata->height**maindata->width*2 ,*maindata->width*2 );
    for (int s = 0; s < maindata->seets; s++)
    {
        for (int y = 0; y < (int)display_height; y++)
        {
            for (int x = 0; x < (int)display_width; x++)
            {
                if ((x > ((int)display_width - (int)maindata->width[s] - 1)) && (y > ((int)display_height - (int)maindata->height[s] - 1)))
                {
                    display_data[((s * display_width * display_height) + (y * display_width) + x) * 2] = maindata->file_data[((s * *maindata->width * *maindata->height) + ((y - (display_height - *maindata->height)) * *maindata->width) + (x - (display_width - *maindata->width))) * 2];
                    display_data[((s * display_width * display_height) + (y * display_width) + x) * 2 + 1] = maindata->file_data[((s * *maindata->width * *maindata->height) + ((y - (display_height - *maindata->height)) * *maindata->width) + (x - (display_width - *maindata->width))) * 2 + 1];
                }
                else
                {
                    display_data[((s * display_width * display_height) + (y * display_width) + x) * 2] = 0x00;
                    display_data[(((s * display_width * display_height) + (y * display_width) + x) * 2) + 1] = 0x00;
                }
            }
        }
    }
    // datalook(display_data ,display_width*display_height*2 ,display_width*2 );
}

void display_image_write(unsigned char *display_data, unsigned int x, unsigned int y, size_t size)
{
    // size=書きこむ枠数

    // オーバーラン対策
    if (size > (((size_t)display_width * display_height) - ((size_t)(y * display_width) + x)) * 2)
    {
        exit(-1);
    }

    int i = 0;

    for (int j = y; j < display_height; j++)
    {
        for (int k = x; k < display_width; k++)
        {

            LED_data.R[(j * display_width) + k] = display_data[i * 2] & 0x0f;
            LED_data.G[(j * display_width) + k] = (display_data[i * 2] & 0xf0) >> 4;
            LED_data.B[(j * display_width) + k] = display_data[i * 2 + 1] & 0x0f;

            if ((display_data[i * 2 + 1] & 0xf0) == 0)
            {
                LED_data.R[(j * display_width) + k] = 0;
                LED_data.G[(j * display_width) + k] = 0;
                LED_data.B[(j * display_width) + k] = 0;
            }

            i++;
        }
    }
}

void calloc_check_int(int **point, size_t size)
{
    // size=確保するメモリサイズ
    int *buf = NULL;
    buf = (int *)malloc(size);
    if (buf == NULL)
    {
        exit(calloc_NULL);
    }
    *point = buf;
}

void calloc_check_uchar(unsigned char **point, size_t size)
{
    // size=確保するメモリサイズ
    unsigned char *buf = NULL;
    buf = (unsigned char *)malloc(size);
    if (buf == NULL)
    {
        exit(calloc_NULL);
    }
    *point = buf;
}

void *LED_drive_A(void *data)
{

    while (1)
    {
        for (multi_finish.count = 0; multi_finish.count < 64; ++multi_finish.count)
        {
            for (multi_finish.y = 0; multi_finish.y < (display_height / 2); ++multi_finish.y)
            {
                for (multi_finish.x = 0; multi_finish.x < display_width; ++multi_finish.x)
                {

                    digitalWrite(R1, 0x0001 & (pattern[LED_data.R[multi_finish.y * display_width + multi_finish.x]] >> multi_finish.count));
                    digitalWrite(G1, 0x0001 & (pattern[LED_data.G[multi_finish.y * display_width + multi_finish.x]] >> multi_finish.count));
                    digitalWrite(B1, 0x0001 & (pattern[LED_data.B[multi_finish.y * display_width + multi_finish.x]] >> multi_finish.count));
                    digitalWrite(R2, 0x0001 & (pattern[LED_data.R[multi_finish.y * display_width + multi_finish.x + display_width * display_height / 2]] >> multi_finish.count));
                    digitalWrite(G2, 0x0001 & (pattern[LED_data.G[multi_finish.y * display_width + multi_finish.x + display_width * display_height / 2]] >> multi_finish.count));
                    digitalWrite(B2, 0x0001 & (pattern[LED_data.B[multi_finish.y * display_width + multi_finish.x + display_width * display_height / 2]] >> multi_finish.count));

                    digitalWrite(CLK, HIGH);
                    delayMicroseconds(5);
                    digitalWrite(CLK, LOW);
                }

                digitalWrite(A, (multi_finish.y & 0b00000001));
                digitalWrite(B, (multi_finish.y & 0b00000010) >> 1);
                digitalWrite(C, (multi_finish.y & 0b00000100) >> 2);
                digitalWrite(D, (multi_finish.y & 0b00001000) >> 3);

                digitalWrite(OE, HIGH);
                digitalWrite(LAT, HIGH);
                delayMicroseconds(1);
                digitalWrite(LAT, LOW);
                digitalWrite(OE, LOW);
            }
        }
    }
}

#ifndef pi
void pinMode(int pin, int status)
{
    LED_status[pin].LED_io = (status != 0);
}

void digitalWrite(int pin, int status)
{
    if (LED_status[pin].LED_io == OUTPUT)
    {
        LED_status[pin].LED_output = (status != 0);
    }
    else
    {
        printf("LED_NOT_OUTPUT");
    }
}

void delay(size_t time)
{
}

void delayMicroseconds(size_t time)
{
}

void wiringPiSetup(void)
{
}
#endif

void datalook(unsigned char *point, size_t size, size_t enter)
{
    for (size_t i = 0; i < size; i++)
    {
        printf("%02x", point[i]);
        if ((i + 1) % enter == 0)
        {
            printf("\n");
        }
    }
}

void print(void)
{
    printf("%d\n", debug);
    fflush(stdout);
    debug++;
}

void pin_setup(void)
{

    wiringPiSetup();
    pinMode(G1, OUTPUT);
    pinMode(G2, OUTPUT);
    pinMode(R1, OUTPUT);
    pinMode(R2, OUTPUT);
    pinMode(B1, OUTPUT);
    pinMode(B2, OUTPUT);
    pinMode(A, OUTPUT);
    pinMode(B, OUTPUT);
    pinMode(C, OUTPUT);
    pinMode(D, OUTPUT);
    pinMode(CLK, OUTPUT);
    pinMode(LAT, OUTPUT);
    pinMode(OE, OUTPUT);
}

void ALLpin_LOW(void)
{
    digitalWrite(G1, LOW);
    digitalWrite(G2, LOW);
    digitalWrite(R1, LOW);
    digitalWrite(R2, LOW);
    digitalWrite(B1, LOW);
    digitalWrite(B2, LOW);
    digitalWrite(A, LOW);
    digitalWrite(B, LOW);
    digitalWrite(C, LOW);
    digitalWrite(D, LOW);
    digitalWrite(CLK, LOW);
    digitalWrite(LAT, LOW);
    digitalWrite(OE, LOW);
}