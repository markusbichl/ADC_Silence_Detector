/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2023/12/26
 * Description        : Main program body.
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/*
 *@Note
 *PB1 push-pull output (silence detector output).
 *
 ***Only PA0--PA15 and PC16--PC17 support input pull-down.
 */

#include "debug.h"
#include "ch32x035.h"
#include <stdbool.h>

/* Global define */

#define THRESHOLD_HYST_OFF  100
#define THRESHOLD_HYST_ON   150

/* Global Variable */

/*********************************************************************
 * @fn      ADC_init
 *
 * @brief   Initializes ADC1 for software-triggered single conversions
 *          on channels 6 (PA6) and 8 (PB0).
 */
void ADC_init(void)
{
    ADC_InitTypeDef  ADC_InitStructure  = {0};
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    /* Clocks */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    /* PA6 -> ADC_IN6 (analog input) */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PB0 -> ADC_IN8 (analog input) */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    ADC_DeInit(ADC1);
    ADC_CLKConfig(ADC1, ADC_CLK_Div6);

    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None; /* software trigger */
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = 1; /* one at a time, channel set per-read */
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);
}

/*********************************************************************
 * @fn      ADC_read
 *
 * @brief   Performs a single conversion on the requested channel and
 *          returns the 12-bit result.
 */
uint16_t ADC_read(uint8_t channel)
{
    ADC_RegularChannelConfig(ADC1, channel, 1, ADC_SampleTime_11Cycles);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    return ADC_GetConversionValue(ADC1);
}

/*********************************************************************
 * @fn      GPIO_init
 *
 * @brief   Initializes GPIOB.1 as push-pull output
 *
 * @return  none
 */
void GPIO_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
    uint16_t volume = 0;
    bool output = Bit_RESET;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();
    SDI_Printf_Enable();
    printf("SDI print enabled\r\n");
    printf("SystemClk:%d\r\n", SystemCoreClock);
    printf( "ChipID:%08x\r\n", DBGMCU_GetCHIPID() );
    printf("ADC Silence Detector\r\n");

    ADC_init();
    GPIO_init();

    GPIO_WriteBit(GPIOB, GPIO_Pin_1, output);

    while(1)
    {
        Delay_Ms(500);
        // Read both ADC inputs and get highest volume level
        volume = ADC_read(ADC_Channel_6);
        uint16_t adc_temp = ADC_read(ADC_Channel_8);
        volume = volume > adc_temp ? volume : adc_temp;

        printf("ADC read: %d\r\n", volume);

        if (output == Bit_RESET && volume > THRESHOLD_HYST_ON) {
            output = Bit_SET;
            GPIO_WriteBit(GPIOB, GPIO_Pin_1, output);
        }
        else if (output == Bit_SET && volume < THRESHOLD_HYST_OFF) {
            output = Bit_RESET;
            GPIO_WriteBit(GPIOB, GPIO_Pin_1, output);
        }
    }
}
