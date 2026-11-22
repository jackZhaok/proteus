#ifndef __HCSR04_H
#define __HCSR04_H

#include "stm32f10x.h"

/* 初始化 GPIO + TIM1 / EXTI8 周期计数器 */
void    HCSR04_Init(void);

/* 阻塞式测距（兼容旧接口） */
float   HCSR04_GetValue(void);

/* ---- 非阻塞式测距 ---- */
void    HCSR04_StartMeasure(void);   /* 发送触发脉冲，启动测量 */
void    HCSR04_Poll(void);           /* 主循环中轮询，处理 Echo 捕获结果 */
float   HCSR04_GetDistance(void);    /* 取最近一次测量距离 (cm)，无效=-1 */
uint8_t HCSR04_IsNewReady(void);     /* 有新结果返回1并清标志 */

uint16_t HCSR04_GetEchoTicks(void);

#endif
