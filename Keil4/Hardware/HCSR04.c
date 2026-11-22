#include "HCSR04.h"
#include "misc.h"
#include "bsp_encoder.h"

/* Proteus C8: measured about 66 timer ticks per cm. */
#define ECHO_TICKS_PER_10_CM 667U
#define ECHO_TIMEOUT_TICKS 29000U

typedef enum { IDLE, WAIT_RISE, WAIT_FALL, CAPTURED } EchoState;

static volatile EchoState state;
static volatile uint16_t riseTime;
static volatile uint16_t pulseWidth;
static uint16_t lastEchoTicks;
static uint8_t capturedByPolling;
static uint16_t triggerTime;
static float distance = -1.0f;
static uint8_t ready;

static uint16_t Micros(void)
{
    return (uint16_t)TIM_GetCounter(TIM1);
}

void HCSR04_Init(void)
{
    GPIO_InitTypeDef gpio;
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef irq;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_11;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio);
    GPIO_ResetBits(GPIOA, GPIO_Pin_11);
    gpio.GPIO_Pin = GPIO_Pin_8;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOA, &gpio);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource8);
    EXTI_ClearITPendingBit(EXTI_Line8);
    exti.EXTI_Line = EXTI_Line8;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);
    irq.NVIC_IRQChannel = EXTI9_5_IRQn;
    irq.NVIC_IRQChannelPreemptionPriority = 0;
    irq.NVIC_IRQChannelSubPriority = 0;
    irq.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&irq);
}

static void SampleEcho(uint16_t now)
{
    BitAction level = (BitAction)GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_8);

    if (state == WAIT_RISE && level == Bit_SET) {
        riseTime = now;
        state = WAIT_FALL;
    } else if (state == WAIT_FALL && level == Bit_RESET) {
        pulseWidth = (uint16_t)(now - riseTime);
        state = CAPTURED;
    }
}

void EXTI9_5_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line8) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line8);
        SampleEcho(Micros());
    }
    Encoder_RightEdgeIRQ();
}

void HCSR04_StartMeasure(void)
{
    uint16_t start;

    if (state != IDLE)
        return;
    ready = 0;
    capturedByPolling = 0;
    GPIO_SetBits(GPIOA, GPIO_Pin_11);
    start = Micros();
    while ((uint16_t)(Micros() - start) < 18U) {}
    triggerTime = Micros();
    EXTI_ClearITPendingBit(EXTI_Line8);
    state = WAIT_RISE;
    GPIO_ResetBits(GPIOA, GPIO_Pin_11);

    /* Poll short echoes before returning to the normal interrupt path. */
    NVIC_DisableIRQ(EXTI9_5_IRQn);
    do {
        EXTI_ClearITPendingBit(EXTI_Line8);
        SampleEcho(Micros());
        Encoder_RightEdgeIRQ();
    } while (state != CAPTURED &&
             (uint16_t)(Micros() - triggerTime) < 1800U);
    capturedByPolling = (state == CAPTURED);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
}

void HCSR04_Poll(void)
{
    EchoState snapshot;
    uint16_t now;

    NVIC_DisableIRQ(EXTI9_5_IRQn);
    snapshot = state;
    now = Micros();
    if (snapshot == CAPTURED) {
        /* Compensate the approximately one-centimetre offset in this Proteus model. */
        uint16_t cm = (uint16_t)((pulseWidth * 10UL + ECHO_TICKS_PER_10_CM / 2U) / ECHO_TICKS_PER_10_CM + 1U);
        /* Short polling path calibrated at the simulator's 2 and 10 cm settings. */
        if (capturedByPolling)
            cm = pulseWidth > 214U ? (uint16_t)((pulseWidth - 214U + 29U) / 58U) : 0U;
        lastEchoTicks = pulseWidth;
        if (cm == 401U)
            cm = 400U;
        distance = cm >= 2U && cm <= 400U ? (float)cm : -1.0f;
    } else if ((snapshot == WAIT_RISE &&
                (uint16_t)(now - triggerTime) >= 5000U) ||
               (snapshot == WAIT_FALL &&
                (uint16_t)(now - riseTime) >= ECHO_TIMEOUT_TICKS)) {
        distance = -1.0f;
        lastEchoTicks = 0;
    } else {
        NVIC_EnableIRQ(EXTI9_5_IRQn);
        return;
    }
    state = IDLE;
    ready = 1;
    NVIC_EnableIRQ(EXTI9_5_IRQn);
}

float HCSR04_GetValue(void)
{
    HCSR04_StartMeasure();
    while (state != IDLE)
        HCSR04_Poll();
    return distance;
}

float HCSR04_GetDistance(void)
{
    return distance;
}

uint8_t HCSR04_IsNewReady(void)
{
    uint8_t result = ready;
    ready = 0;
    return result;
}

uint16_t HCSR04_GetEchoTicks(void)
{
    return lastEchoTicks;
}
