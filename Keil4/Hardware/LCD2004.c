#include "LCD2004.h"
#include "Delay.h"

#define LCD_RS GPIO_Pin_14
#define LCD_EN GPIO_Pin_15
#define LCD_DATA (GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13)

static void WriteNibble(uint8_t value)
{
    GPIO_ResetBits(GPIOB, LCD_DATA);
    GPIO_SetBits(GPIOB, (uint16_t)((value & 15U) << 10));
    Delay_us(1);
    GPIO_SetBits(GPIOB, LCD_EN);
    Delay_us(1);
    GPIO_ResetBits(GPIOB, LCD_EN);
    Delay_us(1);
}

static void WriteByte(uint8_t data, uint8_t value)
{
    GPIO_WriteBit(GPIOB, LCD_RS, data ? Bit_SET : Bit_RESET);
    WriteNibble(value >> 4);
    WriteNibble(value);
    Delay_us(45);
}

void LCD_Clear(void)
{
    WriteByte(0, 0x01);
    Delay_ms(2);
}

void LCD_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin = LCD_RS | LCD_EN | LCD_DATA;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio);
    GPIO_ResetBits(GPIOB, gpio.GPIO_Pin);

    Delay_ms(50);
    WriteNibble(3);
    Delay_ms(5);
    WriteNibble(3);
    Delay_us(150);
    WriteNibble(3);
    Delay_us(150);
    WriteNibble(2);
    Delay_us(150);
    WriteByte(0, 0x28);
    WriteByte(0, 0x08);
    LCD_Clear();
    WriteByte(0, 0x06);
    WriteByte(0, 0x0C);
}

void LCD_ShowString(uint8_t row, uint8_t column, const char *text)
{
    static const uint8_t address[] = {0x00, 0x40, 0x14, 0x54};

    if (row < 1 || row > 4 || column < 1 || column > 20)
        return;

    WriteByte(0, (uint8_t)(0x80 | (address[row - 1] + column - 1)));
    for (; column <= 20; ++column)
        WriteByte(1, (uint8_t)(*text ? *text++ : ' '));
}
