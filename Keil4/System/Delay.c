#include "stm32f10x.h"
#include "Delay.h"

void Delay_us(uint32_t us)
{
    while (us != 0U) {
        uint16_t chunk = (uint16_t)(us > 30000U ? 30000U : us);
        uint16_t start = TIM_GetCounter(TIM1);
        while ((uint16_t)(TIM_GetCounter(TIM1) - start) < chunk) {}
        us -= chunk;
    }
}

void Delay_ms(uint32_t ms)
{
    while (ms-- != 0U)
        Delay_us(1000);
}
