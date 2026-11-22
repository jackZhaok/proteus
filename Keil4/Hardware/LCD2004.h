#ifndef LCD2004_H
#define LCD2004_H

#include "stm32f10x.h"

void LCD_Init(void);
void LCD_Clear(void);
void LCD_ShowString(uint8_t row, uint8_t column, const char *text);

#endif
