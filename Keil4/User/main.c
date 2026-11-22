#include "stm32f10x.h"
#include "bsp_motor.h"
#include "bsp_encoder.h"
#include "HCSR04.h"
#include "LCD2004.h"
#include <stdio.h>

#define SPEED_PERIOD_MS 20U
#define SENSOR_PINS (GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_8 | GPIO_Pin_9)

typedef struct {
    float integral;
    float previousSpeed;
} SpeedPid;

typedef enum { TRACK, TURN_OUT, PASS, FIND_LINE } DriveMode;

static volatile uint32_t milliseconds;
static const uint16_t irPins[] = { GPIO_Pin_0, GPIO_Pin_1, GPIO_Pin_8, GPIO_Pin_9 };
static const int8_t weights[] = { -3, -1, 1, 3 };

void SysTick_Handler(void)
{
    ++milliseconds;
}

static void Board_Init(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef timer;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_TIM1, ENABLE);
    gpio.GPIO_Pin = SENSOR_PINS;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio);

    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = (uint16_t)(SystemCoreClock / 1000000U - 1U);
    timer.TIM_Period = 0xFFFF;
    TIM_TimeBaseInit(TIM1, &timer);
    TIM_Cmd(TIM1, ENABLE);
    SysTick_Config(SystemCoreClock / 1000U);
}

static int16_t Speed_Update(SpeedPid *pid, float target, float actual, float dt)
{
    float error = target - actual;
    float duty;

    pid->integral += error * dt;
    if (pid->integral > 90.0f) pid->integral = 90.0f;
    if (pid->integral < -90.0f) pid->integral = -90.0f;
    duty = 0.18f * error + 0.9f * pid->integral
         - 0.0005f * (actual - pid->previousSpeed) / dt;
    pid->previousSpeed = actual;
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 100.0f) duty = 100.0f;
    return (int16_t)duty;
}

/* The simulated 360 rpm motor cannot exceed 900 mm/s with a 150 mm wheel circumference. */
static float FilterWheelSpeed(float previous, float measured)
{
    if (!(measured >= 0.0f && measured <= 1000.0f))
        return previous;
    return 0.7f * previous + 0.3f * measured;
}

int main(void)
{
    SpeedPid leftPid = {0}, rightPid = {0};
    DriveMode mode = TRACK;
    uint32_t lastMeasure = 0, lastSpeed = 0, lastDisplay = 0, modeStart = 0;
    float distance = -1.0f, leftSpeed = 0.0f, rightSpeed = 0.0f;
    float lastError = 1.0f;
    uint8_t mask = 0, displayRow = 0;
    int16_t leftDuty = 0, rightDuty = 0;
    char line[24];

    Board_Init();
    Motor_Init();
    Encoder_Init();
    HCSR04_Init();
    LCD_Init();
    Motor_Enable(1);
    lastSpeed = milliseconds;

    while (1) {
        uint32_t now = milliseconds;
        HCSR04_Poll();
        if (HCSR04_IsNewReady()) {
            distance = HCSR04_GetDistance();
            if (distance >= 2.0f && distance < 20.0f &&
                (mode == TRACK || mode == FIND_LINE)) {
                mode = TURN_OUT;
                modeStart = now;
            }
        }
        if (now - lastMeasure >= 60U) {
            lastMeasure = now;
            HCSR04_StartMeasure();
        }

        if (now - lastSpeed >= SPEED_PERIOD_MS) {
            uint32_t elapsed = now - lastSpeed;
            float dt = elapsed / 1000.0f;
            float base = 210.0f;
            float turn = 0.0f;
            int sum = 0, count = 0, i;

            lastSpeed = now;
            mask = 0;
            for (i = 0; i < 4; ++i) {
                if (GPIO_ReadInputDataBit(GPIOB, irPins[i]) == Bit_RESET) {
                    mask |= (uint8_t)(1U << (3 - i));
                    sum += weights[i];
                    ++count;
                }
            }

            if (mode == TURN_OUT && now - modeStart >= 400U &&
                (distance < 0.0f || distance >= 25.0f)) {
                mode = PASS;
                modeStart = now;
            }
            if (mode == PASS && now - modeStart >= 500U)
                mode = FIND_LINE;
            if (mode == FIND_LINE && count != 0)
                mode = TRACK;

            if (mode == TURN_OUT) {
                base = 110.0f;
                turn = 100.0f;
            } else if (mode == FIND_LINE) {
                base = 110.0f;
                turn = -90.0f;
            } else if (mode == TRACK) {
                float error;
                if (count != 0) {
                    error = (float)sum / count;
                    if (error != 0.0f)
                        lastError = error;
                } else {
                    error = lastError > 0.0f ? 3.0f : -3.0f;
                    base = 135.0f;
                }
                turn = 40.0f * error;
            }

            Encoder_Update(elapsed);
            leftSpeed = FilterWheelSpeed(leftSpeed, Encoder_GetSpeedMMS(ENCODER_LEFT));
            rightSpeed = FilterWheelSpeed(rightSpeed, Encoder_GetSpeedMMS(ENCODER_RIGHT));
            leftDuty = Speed_Update(&leftPid, base + turn, leftSpeed, dt);
            rightDuty = Speed_Update(&rightPid, base - turn, rightSpeed, dt);
            Motor_SetSpeedBoth(leftDuty, rightDuty);
        }

        if (now - lastDisplay >= 100U) {
            lastDisplay = now;
            switch (displayRow) {
            case 0:
                if (distance > 0.0f)
                    sprintf(line, "%s %3ucm", mode == TRACK ? "TRACK" : "AVOID", (unsigned)distance);
                else
                    sprintf(line, "%s --- E:%u", mode == TRACK ? "TRACK" : "AVOID",
                            (unsigned)HCSR04_GetEchoTicks());
                break;
            case 1:
                snprintf(line, sizeof line, "L:%4u R:%4u mm/s",
                         (unsigned)leftSpeed, (unsigned)rightSpeed);
                break;
            case 2:
                sprintf(line, "PWM L:%3d R:%3d", leftDuty, rightDuty);
                break;
            default:
                sprintf(line, "IR: %c%c%c%c", mask & 8 ? '*' : '-', mask & 4 ? '*' : '-',
                        mask & 2 ? '*' : '-', mask & 1 ? '*' : '-');
                break;
            }
            LCD_ShowString((uint8_t)(displayRow + 1U), 1, line);
            displayRow = (uint8_t)((displayRow + 1U) % 4U);
        }
    }
}
