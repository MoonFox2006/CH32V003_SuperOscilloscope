#define USE_UART

#include <string.h>
#include <stdlib.h>
#include <ch32v00x.h>
#ifdef USE_SPL
#include <ch32v00x_rcc.h>
#include <ch32v00x_gpio.h>
#include <ch32v00x_adc.h>
#endif
#include "utils.h"
#include "twi.h"
#include "ssd1306.h"
#include "encoder.h"
#include "strutils.h"
#ifdef USE_UART
#include "uart.h"
#endif

#ifndef VREF
#define VREF    120 // ~1.2 V
#endif

static void adcInit(void) {
#ifdef USE_SPL
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOC, ENABLE);

    {
        GPIO_InitTypeDef GPIO_InitStructure = { 0 };

        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
        GPIO_InitStructure.GPIO_Pin = 4; // C4
        GPIO_Init(GPIOD, &GPIO_InitStructure);
    }

    RCC_ADCCLKConfig(RCC_PCLK2_Div8);

    {
        ADC_InitTypeDef ADC_InitStructure = { 0 };

        ADC_DeInit(ADC1);
        ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
        ADC_InitStructure.ADC_ScanConvMode = ENABLE;
        ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
        ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigInjecConv_None;
        ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
        ADC_InitStructure.ADC_NbrOfChannel = 1;
        ADC_Init(ADC1, &ADC_InitStructure);
    }

    ADC_Calibration_Vol(ADC1, ADC_CALVOL_50PERCENT);
    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1)) {}
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1)) {}

    ADC_InjectedSequencerLengthConfig(ADC1, 4);
    for (uint8_t i = 1; i <= 3; ++i) {
        ADC_InjectedChannelConfig(ADC1, ADC_Channel_2, i, ADC_SampleTime_241Cycles);
    }
    ADC_InjectedChannelConfig(ADC1, ADC_Channel_Vrefint, 4, ADC_SampleTime_241Cycles);
#else
    RCC->APB2PCENR |= (RCC_ADC1EN | RCC_IOPCEN);

    UPDATE_REG32(&GPIOC->CFGLR, ~(0x0F << (4 * 4)), 0x00 << (4 * 4));
    UPDATE_REG32(&RCC->CFGR0, 0xFFFF07FF, 0x0000C000); // CFGR0_ADCPRE_Reset_Mask, DIV8
    RCC->APB2PRSTR |= RCC_ADC1RST;
    RCC->APB2PRSTR &= ~RCC_ADC1RST;

    UPDATE_REG32(&ADC1->CTLR1, 0xFFF0FEFF, 0 | (1 << 8)); // CTLR1_CLEAR_Mask
    UPDATE_REG32(&ADC1->CTLR2, 0xFFF1F7FD, (ADC_JEXTSEL_0 | ADC_JEXTSEL_1 | ADC_JEXTSEL_2) | (0 << 1)); // CTLR2_CLEAR_Mask
    UPDATE_REG32(&ADC1->RSQR1, 0xFF0FFFFF, (1 - 1) << 20); // RSQR1_CLEAR_Mask

    UPDATE_REG32(&ADC1->CTLR1, ~(uint32_t)(3 << 25), ADC_CALVOLSELECT_0);
    ADC1->CTLR2 |= ADC_ADON;

    ADC1->CTLR2 |= ADC_RSTCAL;
    while (ADC1->CTLR2 & ADC_RSTCAL) {}
    ADC1->CTLR2 |= ADC_CAL;
    while (ADC1->CTLR2 & ADC_CAL) {}

    ADC1->SAMPTR2 |= ((0x07 << (3 * 8)) | (0x07 << (3 * 2)));
    ADC1->ISQR = (3 << 20) | (8 << 15) | (2 << 10) | (2 << 5) | (2 << 0);
#endif
}

static uint16_t adcRead(void) {
    uint32_t result;

#ifdef USE_SPL
    ADC_SoftwareStartInjectedConvCmd(ADC1, ENABLE);
    while (! ADC_GetFlagStatus(ADC1, ADC_FLAG_JEOC)) {}
    result = ADC_GetInjectedConversionValue(ADC1, ADC_InjectedChannel_1);
    result += ADC_GetInjectedConversionValue(ADC1, ADC_InjectedChannel_2);
    result += ADC_GetInjectedConversionValue(ADC1, ADC_InjectedChannel_3);
    return (result * VREF / 3) / ADC_GetInjectedConversionValue(ADC1, ADC_InjectedChannel_4);
#else
    ADC1->CTLR2 |= (ADC_JSWSTART | ADC_JEXTTRIG);
    while (! (ADC1->STATR & ADC_JEOC)) {}
    result = (uint16_t)ADC1->IDATAR1;
    result += (uint16_t)ADC1->IDATAR2;
    result += (uint16_t)ADC1->IDATAR3;
    return (result * VREF / 3) / (uint16_t)ADC1->IDATAR4;
#endif
}

static void voltageStr(char *str, uint16_t voltage, const char *suffix) {
    char *s;

    s = u16str(str, voltage / 100, 1);
    *s++ = '.';
    s = u16str(s, voltage % 100, 10);
    if (suffix) {
        strcpy(s, suffix);
    } else {
        *s = '\0';
    }
}

void voltage(void) {
    uint64_t totalVoltage = 0;
    uint32_t totalTime = 0;
    uint16_t minVoltage = 0xFFFF, maxVoltage = 0;

    adcInit();

#ifdef USE_UART
    uartInit();
#endif

    while (1) {
        char str[16];
        uint16_t voltage;

        if (Encoder_Read() == ENC_BTNCLICK) { // Clear totals
            totalVoltage = 0;
            totalTime = 0;
            minVoltage = 0xFFFF;
            maxVoltage = 0;
        }

        voltage = adcRead();
        totalVoltage += voltage;
        ++totalTime;
        if (voltage < minVoltage)
            minVoltage = voltage;
        if (voltage > maxVoltage)
            maxVoltage = voltage;

        screen_clear();
        voltageStr(str, voltage, " V");
        screen_printstr_x2(str, 0, 0, 1);
        voltageStr(str, minVoltage, " V MIN");
        screen_printstr_x2(str, 0, 16, 1);
        voltageStr(str, maxVoltage, " V MAX");
        screen_printstr_x2(str, 0, 32, 1);
        voltageStr(str, totalVoltage / totalTime, " V AVG");
        screen_printstr_x2(str, 0, 48, 1);
        ssd1306_flush(true);

#ifdef USE_UART
        utoa(voltage, str, 10);
        uartPrint(str);
        uartPrint("\r\n");
#endif
    }
}
