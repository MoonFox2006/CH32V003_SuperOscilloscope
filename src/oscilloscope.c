#define USE_SINUS

#include <string.h>
#include <ch32v00x.h>
#ifdef USE_SPL
#include <ch32v00x_pwr.h>
#include <ch32v00x_rcc.h>
#include <ch32v00x_adc.h>
#include <ch32v00x_gpio.h>
#include <ch32v00x_tim.h>
#endif
#include "utils.h"
#include "twi.h"
#include "ssd1306.h"
#include "encoder.h"
#include "strutils.h"
#include "globals.h"

#define CHART_HEIGHT    55
#define CHART_TOP       (SCREEN_HEIGHT - CHART_HEIGHT)
#define CHART_LEFT      8

#define ADC_FLAG_BUSY   1

#define VREF            120 // 1.2 V

#define DEF_PERIOD      3 // 10 us.

enum { TRIG_FREE, TRIG_RISING, TRIG_FALLING, TRIG_RISING_STOP, TRIG_FALLING_STOP };

static const uint16_t PERIODS[] = {
    1, 2, 5, 10, 25, 50, 100, 250, 500, 1000, 2500, 5000, 10000
};

static void Init_PVD() {
#ifdef USE_SPL
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    PWR_PVDLevelConfig(PWR_PVDLevel_4V4);
    PWR_PVDCmd(ENABLE);
#else
    RCC->APB1PCENR |= RCC_PWREN;
    RCC->APB2PCENR |= RCC_AFIOEN;

//    PWR->CTLR &= ~PWR_CTLR_PLS;
    PWR->CTLR |= (PWR_CTLR_PLS_0 | PWR_CTLR_PLS_1 | PWR_CTLR_PLS_2 | PWR_CTLR_PVDE);
#endif
}

#ifdef USE_SINUS
static void Init_Sinus(uint32_t freq) {
    const uint8_t SINUS[] = {
        63,  79,  93,  106, 116, 123, 126, 125, 120, 112, 100, 86,  71,  55,  40,  26,
        14,  6,   1,   0,   3,   10,  20,  33,  47
    };

#ifdef USE_SPL
    GPIO_InitTypeDef GPIO_InitStructure = { 0 };
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = { 0 };
    TIM_OCInitTypeDef TIM_OCInitStructure = { 0 };
    DMA_InitTypeDef DMA_InitStructure = { 0 };

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_TIM1 | RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_PinRemapConfig(GPIO_PartialRemap1_TIM1, ENABLE); // T1C3 = C0

    TIM_TimeBaseStructure.TIM_Period = 127; // 7 bit PWM
    TIM_TimeBaseStructure.TIM_Prescaler = (48000000 / 128) / (ARRAY_SIZE(SINUS) * freq) - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC3Init(TIM1, &TIM_OCInitStructure);

    TIM_OC3PreloadConfig(TIM1, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);

    DMA_DeInit(DMA1_Channel5);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&TIM1->CH3CVR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)SINUS;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = ARRAY_SIZE(SINUS);
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel5, &DMA_InitStructure);

    DMA_Cmd(DMA1_Channel5, ENABLE);

    TIM_DMACmd(TIM1, TIM_DMA_Update, ENABLE);
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_Cmd(TIM1, ENABLE);
//    TIM_GenerateEvent(TIM1, TIM_EventSource_Update);
#else
    RCC->AHBPCENR |= RCC_DMA1EN;
    RCC->APB2PCENR |= (RCC_AFIOEN | RCC_TIM1EN | RCC_IOPCEN);

    UPDATE_REG32(&GPIOC->CFGLR, ~(0x0F << (0 * 4)), 0x0B << (0 * 4));

    UPDATE_REG32(&AFIO->PCFR1, ~AFIO_PCFR1_TIM1_REMAP, AFIO_PCFR1_TIM1_REMAP_0); // T1C3 = C0

    TIM1->CTLR1 = TIM_ARPE;
    TIM1->ATRLR = 127; // 7 bit PWM
    TIM1->PSC = (48000000 / 128) / (ARRAY_SIZE(SINUS) * freq) - 1;
    TIM1->RPTCR = 0;
    TIM1->SWEVGR = TIM_UG;

    UPDATE_REG16(&TIM1->CCER, ~(TIM_CC3E | TIM_CC3P | TIM_CC3NP | TIM_CC3NE), TIM_CC3E);
    UPDATE_REG16(&TIM1->CTLR2, ~(TIM_OIS3 | TIM_OIS3N), 0);
    UPDATE_REG16(&TIM1->CHCTLR2, ~(TIM_OC3M | TIM_CC3S | TIM_OC3PE), TIM_OC3M_1 | TIM_OC3M_2);
    TIM1->CH3CVR = 0;

    DMA1_Channel5->CFGR = DMA_CFGR1_DIR | DMA_CFGR1_MINC | DMA_CFGR1_PSIZE_0 | DMA_CFGR1_CIRC | DMA_CFGR1_PL_0 | DMA_CFGR1_PL_1;
    DMA1_Channel5->CNTR = ARRAY_SIZE(SINUS);
    DMA1_Channel5->PADDR = (uint32_t)&TIM1->CH3CVR;
    DMA1_Channel5->MADDR = (uint32_t)SINUS;
    DMA1_Channel5->CFGR |= DMA_CFGR1_EN;

    TIM1->DMAINTENR |= TIM_UDE;
    TIM1->BDTR |= TIM_MOE;
    TIM1->CTLR1 |= TIM_CEN;
//    TIM1->SWEVGR = TIM_UG;
#endif
}
#endif

static void Init_ADC(uint16_t us) {
#ifdef USE_SPL
    GPIO_InitTypeDef GPIO_InitStructure = { 0 };
    ADC_InitTypeDef  ADC_InitStructure = { 0 };
    DMA_InitTypeDef DMA_InitStructure = { 0 };
    NVIC_InitTypeDef NVIC_InitStructure = { 0 };

    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOC, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div2); // 24 MHz
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Calibration_Vol(ADC1, ADC_CALVOL_50PERCENT);
    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1)) {}
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1)) {}

    ADC_RegularChannelConfig(ADC1, ADC_Channel_Vrefint, 1, ADC_SampleTime_241Cycles);

    {
        uint32_t sum = 0;

        for (uint8_t i = 0; i <= 10; ++i) {
            ADC_SoftwareStartConvCmd(ADC1, ENABLE);
            while (! ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC)) {}
            if (i) { // Skip first value
                sum += ADC_GetConversionValue(ADC1);
            }
        }
        vars.oscilloscope.vref = sum / 10;
    }

    ADC_Cmd(ADC1, DISABLE);

    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_CC1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 1, us > 10 ? ADC_SampleTime_241Cycles : us > 3 ? ADC_SampleTime_73Cycles : us > 2 ? ADC_SampleTime_57Cycles : us > 1 ? ADC_SampleTime_30Cycles : ADC_SampleTime_9Cycles);
    ADC_ExternalTrig_DLY(ADC1, ADC_ExternalTrigRegul_DLY, 0);
    ADC_ExternalTrigConvCmd(ADC1, ENABLE);

    ADC_Cmd(ADC1, ENABLE);
    ADC_DMACmd(ADC1, ENABLE);

    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->RDATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)vars.oscilloscope.adc_data;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = (vars.oscilloscope.trig == TRIG_FREE) ? ADC_DATA_SIZE : ARRAY_SIZE(vars.oscilloscope.adc_data);
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);

    TIM_TimeBaseStructInit(&TIM_TimeBaseInitStructure);
    TIM_TimeBaseInitStructure.TIM_Prescaler = (FCPU / 1000000) / 4 - 1; // 0.25 us.
    TIM_TimeBaseInitStructure.TIM_Period = (us << 1) - 1;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);

    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Toggle;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 1;
    TIM_OC1Init(TIM2, &TIM_OCInitStructure);

    TIM_CtrlPWMOutputs(TIM2, ENABLE);
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig(TIM2, ENABLE);
//    TIM_Cmd(TIM2, ENABLE);
#else
    RCC->APB1PCENR |= RCC_TIM2EN;
    RCC->APB2PCENR |= (RCC_ADC1EN | RCC_IOPCEN);
    RCC->AHBPCENR |= RCC_DMA1EN;

    UPDATE_REG32(&RCC->CFGR0, 0xFFFF07FF, 0); // CFGR0_ADCPRE_Reset_Mask, 24 MHz

    UPDATE_REG32(&GPIOC->CFGLR, ~(0x0F << (4 * 4)), 0x00 << (4 * 4));

    RCC->APB2PRSTR |= RCC_ADC1RST;
    RCC->APB2PRSTR &= ~RCC_ADC1RST;

    UPDATE_REG32(&ADC1->CTLR1, 0xFFF0FEFF, 0 | (0 << 8)); // CTLR1_CLEAR_Mask
    UPDATE_REG32(&ADC1->CTLR2, 0xFFF1F7FD, (ADC_EXTSEL_0 | ADC_EXTSEL_1 | ADC_EXTSEL_2) | (0 << 1) | ADC_EXTTRIG); // CTLR2_CLEAR_Mask
    UPDATE_REG32(&ADC1->RSQR1, 0xFF0FFFFF, (1 - 1) << 20); // RSQR1_CLEAR_Mask
    UPDATE_REG32(&ADC1->SAMPTR2, ~(0x07 << (3 * 2)), 0x07 << (3 * 2));

    UPDATE_REG32(&ADC1->CTLR1, ~(uint32_t)(3 << 25), ADC_CALVOLSELECT_0);
    ADC1->CTLR2 |= ADC_ADON;

    ADC1->CTLR2 |= ADC_RSTCAL;
    while (ADC1->CTLR2 & ADC_RSTCAL) {}
    ADC1->CTLR2 |= ADC_CAL;
    while (ADC1->CTLR2 & ADC_CAL) {}

    UPDATE_REG32(&ADC1->RSQR3, ~(0x1F << (5 * 0)), (uint32_t)8 << (5 * 0)); // ADC_Channel_Vrefint (8)

    {
        uint32_t sum = 0;

        for (uint8_t i = 0; i <= 10; ++i) {
            ADC1->CTLR2 |= ADC_SWSTART;
            while (! (ADC1->STATR & ADC_EOC)) {}
            if (i) { // Skip first value
                sum += ADC1->RDATAR;
            }
        }
        vars.oscilloscope.vref = sum / 10;
    }

    ADC1->CTLR2 &= ~ADC_ADON;

    UPDATE_REG32(&ADC1->CTLR2, 0xFFF1F7FD, ADC_EXTSEL_2 | (0 << 1) | ADC_EXTTRIG); // CTLR2_CLEAR_Mask
//    UPDATE_REG32(&ADC1->RSQR1, 0xFF0FFFFF, (1 - 1) << 20); // RSQR1_CLEAR_Mask
    UPDATE_REG32(&ADC1->SAMPTR2, ~(0x07 << (3 * 2)),
        (uint32_t)(us > 10 ? 0x07 : us > 3 ? 0x06 : us > 2 ? 0x05 : us > 1 ? 0x03 : 0x01) << (3 * 2));
    UPDATE_REG32(&ADC1->RSQR3, ~(0x1F << (5 * 0)), (uint32_t)2 << (5 * 0));
    ADC1->DLYR &= ~(uint32_t)0x03FF;

    ADC1->CTLR2 |= (ADC_ADON | ADC_DMA);

    DMA1_Channel1->CFGR = DMA_CFGR1_MINC | DMA_CFGR1_PSIZE_0 | DMA_CFGR1_MSIZE_0 | DMA_CFGR1_PL_0 | DMA_CFGR1_PL_1;
    DMA1_Channel1->CNTR = (vars.oscilloscope.trig == TRIG_FREE) ? ADC_DATA_SIZE : ARRAY_SIZE(vars.oscilloscope.adc_data);
    DMA1_Channel1->PADDR = (uint32_t)&ADC1->RDATAR;
    DMA1_Channel1->MADDR = (uint32_t)vars.oscilloscope.adc_data;

    NVIC_SetPriority(DMA1_Channel1_IRQn, (0 << 7) | (1 << 6));
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    DMA1_Channel1->CFGR |= DMA_CFGR1_TCIE;

    TIM2->CTLR1 = TIM_ARPE;
    TIM2->ATRLR = (us << 1) - 1;
    TIM2->PSC = (FCPU / 1000000) / 4 - 1; // 0.25 us.
    TIM2->SWEVGR = TIM_UG;

    UPDATE_REG16(&TIM2->CCER, ~(TIM_CC1E | TIM_CC1P), TIM_CC1E);
    UPDATE_REG16(&TIM2->CHCTLR1, ~(TIM_OC1M | TIM_CC1S | TIM_OC1PE), TIM_OC1M_1 | TIM_OC1M_0);
    TIM2->CH1CVR = 1;

    TIM2->BDTR |= TIM_MOE;
//    TIM2->CTLR1 |= TIM_CEN;
#endif
}

static void Reinit_ADC(uint16_t us) {
#ifdef USE_SPL
    TIM_Cmd(TIM2, DISABLE);
    DMA_Cmd(DMA1_Channel1, DISABLE);
    ADC_Cmd(ADC1, DISABLE);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 1, us > 10 ? ADC_SampleTime_241Cycles : us > 3 ? ADC_SampleTime_73Cycles : us > 2 ? ADC_SampleTime_57Cycles : us > 1 ? ADC_SampleTime_30Cycles : ADC_SampleTime_9Cycles);

    ADC_Cmd(ADC1, ENABLE);

    TIM_SetAutoreload(TIM2, (us << 1) - 1);
#else
    TIM2->CTLR1 &= ~TIM_CEN;
    DMA1_Channel1->CFGR &= ~(uint32_t)DMA_CFGR1_EN;
    ADC1->CTLR2 &= ~ADC_ADON;

    UPDATE_REG32(&ADC1->SAMPTR2, ~(0x07 << (3 * 2)),
        (uint32_t)(us > 10 ? 0x07 : us > 3 ? 0x06 : us > 2 ? 0x05 : us > 1 ? 0x03 : 0x01) << (3 * 2));
    UPDATE_REG32(&ADC1->RSQR3, ~(0x1F << (5 * 0)), (uint32_t)2 << (5 * 0));

    ADC1->CTLR2 |= ADC_ADON;

    TIM2->ATRLR = (us << 1) - 1;
    TIM2->SWEVGR = TIM_UG;
#endif
}

static void Start_ADC(bool wait) {
    DMA1_Channel1->MADDR = (uint32_t)vars.oscilloscope.adc_data;
#ifdef USE_SPL
    DMA_SetCurrDataCounter(DMA1_Channel1, (vars.oscilloscope.trig == TRIG_FREE) ? ADC_DATA_SIZE : ARRAY_SIZE(vars.oscilloscope.adc_data));
    DMA_Cmd(DMA1_Channel1, ENABLE);

    TIM_SetCounter(TIM2, 0);
    vars.oscilloscope._adc_flags = ADC_FLAG_BUSY;
    TIM_Cmd(TIM2, ENABLE);
#else
    DMA1_Channel1->CNTR = (vars.oscilloscope.trig == TRIG_FREE) ? ADC_DATA_SIZE : ARRAY_SIZE(vars.oscilloscope.adc_data);
    DMA1_Channel1->CFGR |= DMA_CFGR1_EN;

    TIM2->CNT = 0;
    vars.oscilloscope._adc_flags = ADC_FLAG_BUSY;
    TIM2->CTLR1 |= TIM_CEN;
#endif
    if (wait) {
        while (vars.oscilloscope._adc_flags & ADC_FLAG_BUSY) {}
    }
}

static void norm_time(uint32_t period) {
    const char* const TRIGSTR[] = {
        "~", "/", "\\", "!/", "!\\"
    };

    char *s;

    strcpy(vars.oscilloscope.timestr, TRIGSTR[vars.oscilloscope.trig]);
    s = &vars.oscilloscope.timestr[strlen(vars.oscilloscope.timestr)];
    *s++ = ' ';
    period *= (ADC_DATA_SIZE >> 2);
    if (period < 1000) {
        s = u16str(s, period, 1);
        *s++ = 'u';
    } else {
        if ((period % 1000) == 0) {
            s = u16str(s, period / 1000, 1);
        } else {
            s = u16str(s, period / 1000, 1);
            *s++ = '.';
            s = u16str(s, (period % 1000) / 100, 1);
        }
        *s++ = 'm';
    }
    *s = '\0';
}

static inline uint16_t trim(uint16_t data) {
    return (data > 1023) ? 1023 : data;
}

static void find_range(uint16_t *min_value, uint16_t *max_value, uint8_t start, uint8_t size) {
    *min_value = vars.oscilloscope.adc_data[start];
    *max_value = vars.oscilloscope.adc_data[start];
    ++start;

    while (--size) {
        if (vars.oscilloscope.adc_data[start] < *min_value)
            *min_value = vars.oscilloscope.adc_data[start];
        if (vars.oscilloscope.adc_data[start] > *max_value)
            *max_value = vars.oscilloscope.adc_data[start];
        ++start;
    }
}

static bool draw_screen(bool plot, bool wait) {
    char str[22];
    char *s;
    uint16_t adc_min, adc_max;
    uint16_t trig_value;
    bool result = false;
    uint8_t trig_start = 0;

    screen_clear();
    screen_vline(CHART_LEFT - 1, CHART_TOP, CHART_HEIGHT, 1);
    if (vars.oscilloscope.vcc == 500) { // 5.0 V
        for (uint8_t y = 5 + CHART_TOP; y < SCREEN_HEIGHT; y += 11) {
            for (uint8_t x = 4; x < SCREEN_WIDTH; x += 8) {
                screen_hline(x, y, 3, 1);
            }
        }
    } else { // 3.3 V
        for (uint8_t y = 10 + CHART_TOP; y < SCREEN_HEIGHT; y += 17) {
            for (uint8_t x = 4; x < SCREEN_WIDTH; x += 8) {
                screen_hline(x, y, 3, 1);
            }
        }
    }
    for (uint8_t x = CHART_LEFT + 30; x < SCREEN_WIDTH; x += 30) {
        for (uint8_t y = CHART_TOP + 2; y < SCREEN_HEIGHT; y += 8) {
            screen_vline(x, y, 3, 1);
        }
    }

    if (vars.oscilloscope.trig != TRIG_FREE) {
        find_range(&adc_min, &adc_max, ADC_TRIG_SIZE, ADC_DATA_SIZE);
        trig_value = (adc_max - adc_min + 1) >> 1; // /2
        trig_start = ADC_TRIG_SIZE;
        while (trig_start < ADC_DATA_SIZE) {
            if ((vars.oscilloscope.trig == TRIG_RISING) || (vars.oscilloscope.trig == TRIG_RISING_STOP)) {
                if ((vars.oscilloscope.adc_data[trig_start] < trig_value) && (vars.oscilloscope.adc_data[trig_start + 1] >= trig_value) &&
                    (vars.oscilloscope.adc_data[trig_start - 1] <= vars.oscilloscope.adc_data[trig_start]) && (vars.oscilloscope.adc_data[trig_start + 2] >= vars.oscilloscope.adc_data[trig_start + 1])) {
                    result = true;
                    break;
                }
            } else { // (vars.oscilloscope.trig == TRIG_FALLING) || (vars.oscilloscope.trig == TRIG_FALLING_STOP)
                if ((vars.oscilloscope.adc_data[trig_start] > trig_value) && (vars.oscilloscope.adc_data[trig_start + 1] <= trig_value) &&
                    (vars.oscilloscope.adc_data[trig_start - 1] >= vars.oscilloscope.adc_data[trig_start]) && (vars.oscilloscope.adc_data[trig_start + 2] <= vars.oscilloscope.adc_data[trig_start + 1])) {
                    result = true;
                    break;
                }
            }
            ++trig_start;
        }
        trig_start -= ADC_TRIG_SIZE;
    }
    find_range(&adc_min, &adc_max, trig_start, ADC_DATA_SIZE);
    if (adc_max - adc_min >= 1023)
        trig_value = 0;
    else
        trig_value = (1023 - (adc_max - adc_min)) >> 1;
    for (uint8_t i = 0; i < ADC_DATA_SIZE; ++i) {
        uint8_t index = i + trig_start;

        vars.oscilloscope.adc_data[index] = SCREEN_HEIGHT - 1 - trim(vars.oscilloscope.adc_data[index] - adc_min + trig_value) * CHART_HEIGHT / 1023;
        if (plot) {
            screen_pixel(i + CHART_LEFT, vars.oscilloscope.adc_data[index], 1);
        } else {
            if (i)
                screen_line(i + CHART_LEFT - 1, vars.oscilloscope.adc_data[index - 1], i + CHART_LEFT, vars.oscilloscope.adc_data[index], 1);
        }
    }
#ifdef VREF
    adc_min = adc_min * VREF / vars.oscilloscope.vref;
    adc_max = adc_max * VREF / vars.oscilloscope.vref;
#else
    adc_min = adc_min * vars.oscilloscope.vcc / 1023;
    adc_max = adc_max * vars.oscilloscope.vcc / 1023;
#endif
    strcpy(str, vars.oscilloscope.timestr);
//    sprintf(&str[strlen(str)], "s %u.%02u-%u.%02u", adc_min / 100, adc_min % 100, adc_max / 100, adc_max % 100);
    s = &str[strlen(str)];
    *s++ = 's';
    *s++ = ' ';
    s = u16str(s, adc_min / 100, 1);
    *s++ = '.';
    s = u16str(s, adc_min % 100, 10);
    *s++ = '-';
    s = u16str(s, adc_max / 100, 1);
    *s++ = '.';
    s = u16str(s, adc_max % 100, 10);
    *s = '\0';
    screen_printstr(str, 0, 0, 1);
    ssd1306_flush(wait);
    return result;
}

void oscilloscope(void) {
    uint8_t period = DEF_PERIOD;
    bool ready;

    vars.oscilloscope.trig = TRIG_RISING;

    Init_PVD();

    Init_ADC(PERIODS[period]);

#ifdef USE_SINUS
    Init_Sinus(1000);
#endif

    Start_ADC(false);
    ready = true;

    norm_time(PERIODS[period]);

#ifdef USE_SPL
    if (PWR_GetFlagStatus(PWR_FLAG_PVDO)) // Below 4.4 V
#else
    if (PWR->CSR & PWR_CSR_PVDO) // Below 4.4 V
#endif
        vars.oscilloscope.vcc = 330; // 3.3 V
    else
        vars.oscilloscope.vcc = 500; // 5.0 V

    while (1) {
        uint8_t e;

        e = Encoder_Read();
        if (e != ENC_NONE) {
            switch (e) {
                case ENC_CCW: // Decrement period
                    if (period > 0)
                        --period;
                    else
                        period = ARRAY_SIZE(PERIODS) - 1;
                    break;
                case ENC_CW: // Increment period
                    if (period < ARRAY_SIZE(PERIODS) - 1)
                        ++period;
                    else
                        period = 0;
                    break;
                case ENC_BTNCCW: // Decrement trigger mode
                    if (vars.oscilloscope.trig > TRIG_FREE)
                        --vars.oscilloscope.trig;
                    else
                        vars.oscilloscope.trig = TRIG_FALLING_STOP;
                    break;
                case ENC_BTNCW: // Increment trigger mode
                    if (vars.oscilloscope.trig < TRIG_FALLING_STOP)
                        ++vars.oscilloscope.trig;
                    else
                        vars.oscilloscope.trig = TRIG_FREE;
                    break;
            }
            if (e != ENC_BTNCLICK) { // Encoder
                Reinit_ADC(PERIODS[period]);
                Start_ADC(false);
                ready = true;
                norm_time(PERIODS[period]);
            } else { // Button
                if (! ready) {
                    Start_ADC(false);
                    ready = true;
                }
            }
        }

        if (ready && (! (vars.oscilloscope._adc_flags & ADC_FLAG_BUSY))) {
            ready = (! draw_screen(false, PERIODS[period] <= 100)) || ((vars.oscilloscope.trig != TRIG_RISING_STOP) && (vars.oscilloscope.trig != TRIG_FALLING_STOP));
            if (ready)
                Start_ADC(false);
        }
    }
}

void __attribute__((interrupt("WCH-Interrupt-fast"))) DMA1_Channel1_IRQHandler() {
#ifdef USE_SPL
    if (DMA_GetITStatus(DMA1_IT_TC1)) {
        TIM_Cmd(TIM2, DISABLE);
        DMA_Cmd(DMA1_Channel1, DISABLE);
        DMA_ClearITPendingBit(DMA1_IT_TC1);
        vars.oscilloscope._adc_flags = 0;
    }
#else
    if (DMA1->INTFR & DMA_TCIF1) {
        TIM2->CTLR1 &= ~TIM_CEN;
        DMA1_Channel1->CFGR &= ~(uint32_t)DMA_CFGR1_EN;
        DMA1->INTFCR = DMA_CTCIF1;
        vars.oscilloscope._adc_flags = 0;
    }
#endif
}
