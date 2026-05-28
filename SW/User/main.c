/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : ADC Silence Detector with corrected hysteresis.
*********************************************************************************/

/*
 *@Note
 * PB1 push-pull output (silence detector output, active high).
 * PA6 / ADC_IN6  and  PB0 / ADC_IN8  are the two analog inputs.
 *
 * State machine:
 *   - Loud and changing  -> count up to ON
 *   - Quiet              -> count down to OFF
 *   - Loud but stuck     -> count up to OFF (assume source died with DC offset)
 *   - Dead-band          -> hold current state, hold debounce counters
 */

#include "debug.h"
#include "ch32x035.h"
#include <stdbool.h>
#include <stdint.h>

/* ---- Tunables ---------------------------------------------------------- */

#define SAMPLETIME          100

#define THRESHOLD_HYST_OFF  10      /* below this = silence  */
#define THRESHOLD_HYST_ON   20      /* above this = sound    */
#define STUCK_TOL            4      /* +/- jitter tolerance for "stuck" */

/* Debounce in tick iterations (one tick = one main-loop pass ~= 300 ms) */
#define OFFCNT              10
#define ONCNT                3
#define IRREGULARCNT         5

/* Peak-hold: how many ADC samples per tick.
 * Audio is AC, so a single sample isn't a meaningful "volume". We take the
 * peak over a burst at each tick. Tune to cover the lowest audio period
 * you care about (e.g. for ~50 Hz, you want a window > 20 ms).
 */
#define PEAK_SAMPLES       256


/* ---- ADC --------------------------------------------------------------- */

void ADC_init(void)
{
    ADC_InitTypeDef  ADC_InitStructure  = {0};
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    /* PA6 -> ADC_IN6 */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PB0 -> ADC_IN8 */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    ADC_DeInit(ADC1);
    ADC_CLKConfig(ADC1, ADC_CLK_Div6);

    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);
}

uint16_t ADC_read(uint8_t channel)
{
    ADC_RegularChannelConfig(ADC1, channel, 1, ADC_SampleTime_11Cycles);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    return ADC_GetConversionValue(ADC1);
}

/* Peak-hold over both channels for one tick. */
static uint16_t read_volume_peak(void)
{
    uint16_t peak = 0;
    for (uint16_t i = 0; i < PEAK_SAMPLES; i++) {
        uint16_t s1 = ADC_read(ADC_Channel_6);
        uint16_t s2 = ADC_read(ADC_Channel_8);
        uint16_t s  = (s1 > s2) ? s1 : s2;
        if (s > peak) peak = s;
    }
    return peak;
}


/* ---- GPIO -------------------------------------------------------------- */

void GPIO_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}


/* ---- main -------------------------------------------------------------- */

int main(void)
{
    uint16_t volume        = 0;
    uint16_t volume_old    = 0;
    uint8_t  irregularcntr = IRREGULARCNT;
    uint8_t  offcntr       = OFFCNT;
    uint8_t  oncntr        = ONCNT;
    bool     output        = Bit_RESET;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();
    /*SDI_Printf_Enable();
    printf("SDI print enabled\r\n");
    printf("SystemClk:%d\r\n", SystemCoreClock);
    printf("ChipID:%08x\r\n", DBGMCU_GetCHIPID());
    printf("ADC Silence Detector\r\n");*/

    ADC_init();
    GPIO_init();
    GPIO_WriteBit(GPIOB, GPIO_Pin_1, output);

    while (1)
    {
        Delay_Ms(SAMPLETIME);

        volume = read_volume_peak();

        int16_t diff  = (int16_t)volume - (int16_t)volume_old;
        bool    stuck = (diff > -STUCK_TOL) && (diff < STUCK_TOL);

        /* -- Irregular: output is on, signal loud but not changing.
         *    Assume the source died with a stuck DC level; fall back to off. */
        if (volume > THRESHOLD_HYST_ON && stuck)
        {
                offcntr = OFFCNT;
                oncntr  = ONCNT;
            if (--irregularcntr == 0) {
                output        = Bit_RESET;
                irregularcntr = IRREGULARCNT;
                GPIO_WriteBit(GPIOB, GPIO_Pin_1, output);
            }
        }
        /* -- Rising: output off and signal above the on-threshold.
         *    Count toward ON; keep irregular and off counters armed. */
        else if (volume > THRESHOLD_HYST_ON && !stuck)
        {
            irregularcntr = IRREGULARCNT;
            offcntr       = OFFCNT;
            if (--oncntr == 0) {
                output = Bit_SET;
                oncntr = ONCNT;
                GPIO_WriteBit(GPIOB, GPIO_Pin_1, output);
            }
        }
        /* -- Falling: output on and signal below the off-threshold.
         *    Count toward OFF; keep irregular and on counters armed. */
        else if (volume < THRESHOLD_HYST_OFF)
        {
            irregularcntr = IRREGULARCNT;
            oncntr        = ONCNT;
            if (--offcntr == 0) {
                output  = Bit_RESET;
                offcntr = OFFCNT;
                GPIO_WriteBit(GPIOB, GPIO_Pin_1, output);
            }
        }
        /* -- Dead-band or no condition met. Reset irregular but preserve
         *    on/off debounce so brief excursions don't restart counting. */
        else
        {
            irregularcntr = IRREGULARCNT;
        }

        volume_old = volume;

        /*printf("vol=%4u  out=%u  on=%u off=%u irr=%u\r\n",
               volume, output, oncntr, offcntr, irregularcntr);*/
    }
}