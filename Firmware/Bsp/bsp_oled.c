#include "bsp_oled.h"
#include "bsp_delay.h"
#include "oled_font.h"

/* ---------------- 引脚：软件 I2C，PB8=SCL / PB9=SDA ----------------
 * 开漏输出，外部或模块自带上拉。写 1 释放(高)，写 0 拉低。 */
#define OLED_SCL_PIN    GPIO_Pin_8
#define OLED_SDA_PIN    GPIO_Pin_9
#define OLED_GPIO       GPIOB

#define OLED_W_SCL(x)   GPIO_WriteBit(OLED_GPIO, OLED_SCL_PIN, (BitAction)(x))
#define OLED_W_SDA(x)   GPIO_WriteBit(OLED_GPIO, OLED_SDA_PIN, (BitAction)(x))

/* 软件 I2C 位间隔。72MHz 下几微秒足够 SSD1306(<=400kHz)，给点余量 */
#define OLED_I2C_DLY()  delay_us(2)

/* ---------------- 软件 I2C 底层 ---------------- */
static void OLED_I2C_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    gpio.GPIO_Mode  = GPIO_Mode_Out_OD;          /* 开漏 */
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Pin   = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_Init(OLED_GPIO, &gpio);

    OLED_W_SCL(1);
    OLED_W_SDA(1);
}

static void OLED_I2C_Start(void)
{
    OLED_W_SDA(1);
    OLED_W_SCL(1);
    OLED_I2C_DLY();
    OLED_W_SDA(0);
    OLED_I2C_DLY();
    OLED_W_SCL(0);
}

static void OLED_I2C_Stop(void)
{
    OLED_W_SDA(0);
    OLED_W_SCL(1);
    OLED_I2C_DLY();
    OLED_W_SDA(1);
    OLED_I2C_DLY();
}

static void OLED_I2C_SendByte(uint8_t Byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++) {
        OLED_W_SDA(!!(Byte & (0x80 >> i)));
        OLED_I2C_DLY();
        OLED_W_SCL(1);
        OLED_I2C_DLY();
        OLED_W_SCL(0);
    }
    OLED_W_SCL(1);          /* 第 9 个时钟：不检查 ACK */
    OLED_I2C_DLY();
    OLED_W_SCL(0);
}

static void OLED_WriteCommand(uint8_t Command)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);        /* 从机地址 */
    OLED_I2C_SendByte(0x00);        /* 写命令 */
    OLED_I2C_SendByte(Command);
    OLED_I2C_Stop();
}

static void OLED_WriteData(uint8_t Data)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);
    OLED_I2C_SendByte(0x40);        /* 写数据 */
    OLED_I2C_SendByte(Data);
    OLED_I2C_Stop();
}

static void OLED_SetCursor(uint8_t Y, uint8_t X)
{
    OLED_WriteCommand(0xB0 | Y);                    /* 页地址 0~7 */
    OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));    /* 列高 4 位 */
    OLED_WriteCommand(0x00 | (X & 0x0F));           /* 列低 4 位 */
}

/* ---------------- 对外 API ---------------- */
void OLED_Clear(void)
{
    uint8_t i, j;
    for (j = 0; j < 8; j++) {
        OLED_SetCursor(j, 0);
        for (i = 0; i < 128; i++) {
            OLED_WriteData(0x00);
        }
    }
}

void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
    uint8_t i;
    uint8_t idx = (uint8_t)(Char - ' ');            /* 字库从空格起 */

    OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
    for (i = 0; i < 8; i++) {
        OLED_WriteData(OLED_F8x16[idx][i]);         /* 上半 */
    }
    OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
    for (i = 0; i < 8; i++) {
        OLED_WriteData(OLED_F8x16[idx][i + 8]);     /* 下半 */
    }
}

void OLED_ShowString(uint8_t Line, uint8_t Column, const char *String)
{
    uint8_t col = Column;
    while (*String && col <= 16) {
        char c = *String++;
        if (c < ' ' || (uint8_t)c > '~') {
            c = ' ';                                /* 非 ASCII 可见字符用空格顶替 */
        }
        OLED_ShowChar(Line, col, c);
        col++;
    }
}

static uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
    uint32_t r = 1;
    while (Y--) {
        r *= X;
    }
    return r;
}

void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++) {
        OLED_ShowChar(Line, Column + i,
                      (char)(Number / OLED_Pow(10, Length - i - 1) % 10 + '0'));
    }
}

void OLED_ClearLine(uint8_t Line)
{
    uint8_t col;
    for (col = 1; col <= 16; col++) {
        OLED_ShowChar(Line, col, ' ');
    }
}

/* ---------------- 初始化 ---------------- */
void OLED_Init(void)
{
    /* 上电稳压延时。bsp_delay_init() 已在 main 里最先调用 */
    delay_ms(20);

    OLED_I2C_Init();

    OLED_WriteCommand(0xAE);    /* 关显示 */
    OLED_WriteCommand(0xD5); OLED_WriteCommand(0x80);
    OLED_WriteCommand(0xA8); OLED_WriteCommand(0x3F);
    OLED_WriteCommand(0xD3); OLED_WriteCommand(0x00);
    OLED_WriteCommand(0x40);
    OLED_WriteCommand(0xA1);    /* 左右方向 */
    OLED_WriteCommand(0xC8);    /* 上下方向 */
    OLED_WriteCommand(0xDA); OLED_WriteCommand(0x12);
    OLED_WriteCommand(0x81); OLED_WriteCommand(0xCF);
    OLED_WriteCommand(0xD9); OLED_WriteCommand(0xF1);
    OLED_WriteCommand(0xDB); OLED_WriteCommand(0x30);
    OLED_WriteCommand(0xA4);
    OLED_WriteCommand(0xA6);
    OLED_WriteCommand(0x8D); OLED_WriteCommand(0x14);   /* 充电泵 */
    OLED_WriteCommand(0xAF);    /* 开显示 */

    OLED_Clear();
}
