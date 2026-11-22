#include <REG51.H>
#include <intrins.h>

/* Wiring: 12MHz, classic 12-clock AT89C51.
 * P2.0 -> L293D EN1; P2.1 -> IN1; P2.2 -> IN2.
 * LCD: P0 -> D0..D7; P2.3 -> RS; P2.4 -> E; RW -> GND.
 * Keys P1.0..P1.3 -> GND: start, faster, slower, reverse.
 * Encoder Q1 -> P3.2/INT0; model PPR = 24.
 */
sbit EN = P2^0;
sbit IN1 = P2^1;
sbit IN2 = P2^2;
sbit RS = P2^3;
sbit LE = P2^4;

volatile bit drive = 0;
volatile unsigned char duty = 50;
volatile unsigned char phase = 0;
volatile unsigned char tick10 = 0;
volatile unsigned int pulses = 0;
volatile unsigned int sample = 0;
volatile unsigned int quiet = 8000;
volatile bit sample_ready = 0;

bit running = 0;
bit reverse = 0;
bit changing = 0;
unsigned int rpm = 0;

/* Timer0 mode 2: 250us auto-reload at 12MHz.
 * 40 ticks per PWM period = 100Hz, 2.5 percent resolution.
 * User duty changes in 10-percent steps.
 */
void timer0_isr(void) interrupt 1 using 1
{
    static unsigned char div10 = 0;
    static unsigned int second = 0;

    if (drive && phase < (duty / 5) * 2)
        EN = 1;
    else
        EN = 0;

    if (++phase >= 40) phase = 0;
    if (quiet < 8000) quiet++;

    if (++div10 >= 40)
    {
        div10 = 0;
        if (tick10 < 250) tick10++;
    }
    if (++second >= 4000)
    {
        second = 0;
        sample = pulses;
        pulses = 0;
        sample_ready = 1;
    }
}

/* One falling edge per encoder cycle; no quadrature x4 counting. */
void encoder_isr(void) interrupt 0 using 2
{
    pulses++;
    quiet = 0;
}

void delay_ms(unsigned int ms)
{
    unsigned int i;
    while (ms--)
        for (i = 0; i < 120; i++) _nop_();
}

void lcd_write(unsigned char value, unsigned char data_flag)
{
    LE = 0;
    RS = data_flag;
    P0 = value;
    _nop_();
    _nop_();
    LE = 1;
    _nop_();
    _nop_();
    _nop_();
    _nop_();
    LE = 0;
    delay_ms(2);
}

void lcd_init(void)
{
    RS = 0;
    LE = 0;
    delay_ms(30);
    lcd_write(0x38, 0);
    delay_ms(5);
    lcd_write(0x38, 0);
    lcd_write(0x38, 0);
    lcd_write(0x08, 0);
    lcd_write(0x01, 0);
    delay_ms(5);
    lcd_write(0x06, 0);
    lcd_write(0x0C, 0);
}

void text(char *p)
{
    while (*p) lcd_write(*p++, 1);
}

void number(unsigned int n, unsigned int divisor)
{
    while (divisor)
    {
        lcd_write('0' + n / divisor, 1);
        n %= divisor;
        divisor /= 10;
    }
}

void display(void)
{
    lcd_write(0x80, 0);
    if (changing) text("WAIT ");
    else if (running) text("RUN  ");
    else text("STOP ");
    if (reverse) text("REV ");
    else text("FWD ");
    text("D:");
    number(duty, 100);
    text("%  ");
    lcd_write(0xC0, 0);
    text("RPM:");
    number(rpm, 10000);
    text("       ");
}

/* Call on real-time 10ms scheduler ticks; 20ms debounce per key.
 * Return only new press events, not held-key repeats.
 */
unsigned char keys(void)
{
    static unsigned char stable = 0;
    static unsigned char last = 0;
    static unsigned char count[4] = {0,0,0,0};
    unsigned char raw, mask, i, event;
    raw = (~P1) & 0x0F;
    for (i = 0, mask = 1; i < 4; i++, mask <<= 1)
    {
        if ((raw & mask) != (stable & mask))
        {
            if (++count[i] >= 2)
            {
                stable ^= mask;
                count[i] = 0;
            }
        }
        else count[i] = 0;
    }
    event = stable & (~last);
    last = stable;
    return event;
}

void controls(unsigned char k)
{
    unsigned int silence;
    if (k & 1) running = !running;
    if ((k & 2) && duty < 100) duty += 10;
    if ((k & 4) && duty > 0) duty -= 10;

    if ((k & 8) && !changing)
    {
        drive = 0;
        EN = 0;
        changing = 1;
    }
    EA = 0;
    silence = quiet;
    EA = 1;
    /* Require 0.5s without encoder edges before changing direction.
     * This is a simulation heuristic, not certified zero-speed sensing.
     */
    if (changing && silence >= 2000)
    {
        reverse = !reverse;
        IN1 = !reverse;
        IN2 = reverse;
        changing = 0;
    }
    drive = running && !changing;
    if (!drive) EN = 0;
}

void main(void)
{
    unsigned char k;
    unsigned char refresh = 0;
    unsigned int n;
    EN = 0;
    IN1 = 1;
    IN2 = 0;
    P1 = 0xFF;
    P3 |= 0x04;
    P0 = 0xFF;
    lcd_init();

    TMOD = 0x02;
    TH0 = 6;
    TL0 = 6;
    IT0 = 1;
    IE0 = 0;
    TF0 = 0;
    EX0 = 1;
    ET0 = 1;
    TR0 = 1;
    EA = 1;
    display();

    while (1)
    {
        if (sample_ready)
        {
            EA = 0;
            n = sample;
            sample_ready = 0;
            EA = 1;
            /* RPM = pulses * 60 / 24 over a 1s window. */
            rpm = (unsigned int)(((unsigned long)n * 60UL) / 24UL);
        }
        if (tick10)
        {
            /* Do not replay queued samples after LCD blocking writes. */
            EA = 0;
            tick10 = 0;
            EA = 1;
            k = keys();
            controls(k);
            if (++refresh >= 20)
            {
                refresh = 0;
                display();
            }
        }
    }
}
