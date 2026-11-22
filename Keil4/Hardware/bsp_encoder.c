#include "bsp_encoder.h"
#include "misc.h"

Encoder_Data_t g_encoderL = {0};
Encoder_Data_t g_encoderR = {0};
static volatile uint16_t leftEdges, rightEdges;
static uint16_t lastLeft, lastRight;

void Encoder_Init(void)
{
    GPIO_InitTypeDef gpio;
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef irq;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_6;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource6);
    exti.EXTI_Line = EXTI_Line0 | EXTI_Line6;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Rising;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_ClearITPendingBit(exti.EXTI_Line);
    EXTI_Init(&exti);

    irq.NVIC_IRQChannel = EXTI0_IRQn;
    irq.NVIC_IRQChannelPreemptionPriority = 0;
    irq.NVIC_IRQChannelSubPriority = 0;
    irq.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&irq);
}

void EXTI0_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line0);
        ++leftEdges;
    }
}

void Encoder_RightEdgeIRQ(void)
{
    if (EXTI_GetITStatus(EXTI_Line6) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line6);
        ++rightEdges;
    }
}

void Encoder_Update(uint32_t samplePeriodMs)
{
    uint16_t left = leftEdges, right = rightEdges;
    int16_t countL = (int16_t)(uint16_t)(left - lastLeft);
    int16_t countR = (int16_t)(uint16_t)(right - lastRight);
    float sampleFreq = 1000.0f / samplePeriodMs;

    lastLeft = left;
    lastRight = right;
    /* 更新左电机数据 */
    g_encoderL.count = countL;
    g_encoderL.totalCount += countL;
    g_encoderL.speed = (float)countL;
    /* RPM = (脉冲数 / 每转脉冲数) * 采样频率 * 60 */
    g_encoderL.speedRPM = (float)countL / ENCODER_PPR * sampleFreq * 60.0f;
    /* mm/s = RPM * 周长 / 60 */
    g_encoderL.speedMMS = g_encoderL.speedRPM * WHEEL_CIRCUMFERENCE_MM / 60.0f;
    /* 距离 = 脉冲数 / 每转脉冲数 * 周长 */
    g_encoderL.distance += (float)countL / ENCODER_PPR * WHEEL_CIRCUMFERENCE_MM;
    
    /* 更新右电机数据 */
    g_encoderR.count = countR;
    g_encoderR.totalCount += countR;
    g_encoderR.speed = (float)countR;
    g_encoderR.speedRPM = (float)countR / ENCODER_PPR * sampleFreq * 60.0f;
    g_encoderR.speedMMS = g_encoderR.speedRPM * WHEEL_CIRCUMFERENCE_MM / 60.0f;
    g_encoderR.distance += (float)countR / ENCODER_PPR * WHEEL_CIRCUMFERENCE_MM;
}

/**
  * @brief  获取编码器当前计数值
  */
int16_t Encoder_GetCount(Encoder_Index_t encoder)
{
    if (encoder == ENCODER_LEFT)
        return g_encoderL.count;
    else
        return g_encoderR.count;
}

/**
  * @brief  获取编码器速度 (RPM)
  */
float Encoder_GetSpeedRPM(Encoder_Index_t encoder)
{
    if (encoder == ENCODER_LEFT)
        return g_encoderL.speedRPM;
    else
        return g_encoderR.speedRPM;
}

/**
  * @brief  获取编码器速度 (mm/s)
  */
float Encoder_GetSpeedMMS(Encoder_Index_t encoder)
{
    if (encoder == ENCODER_LEFT)
        return g_encoderL.speedMMS;
    else
        return g_encoderR.speedMMS;
}

/**
  * @brief  获取累计距离 (mm)
  */
float Encoder_GetDistance(Encoder_Index_t encoder)
{
    if (encoder == ENCODER_LEFT)
        return g_encoderL.distance;
    else
        return g_encoderR.distance;
}

/**
  * @brief  重置编码器累计值
  */
void Encoder_Reset(void)
{
    lastLeft = leftEdges;
    lastRight = rightEdges;
    
    g_encoderL.totalCount = 0;
    g_encoderL.distance = 0;
    
    g_encoderR.totalCount = 0;
    g_encoderR.distance = 0;
}
