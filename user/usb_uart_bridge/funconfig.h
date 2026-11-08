#ifndef _FUNCONFIG_H
#define _FUNCONFIG_H

#define FUNCONF_ENABLE_HPE          0
#define FUNCONF_ISR_IN_RAM          1
#define FUNCONF_SYSTICK_USE_HCLK    1
#define FUNCONF_USE_DEBUGPRINTF     0
#define FUNCONF_USE_UARTPRINTF      0
#define FUNCONF_USE_USBPRINTF       0
#define FUNCONF_NULL_PRINTF         1
#define FUNCONF_USE_HSI             0
#define CLK_SOURCE_CH5XX            CLK_SOURCE_PLL_60MHz
#define FUNCONF_SYSTEM_CORE_CLOCK   (60 * 1000 * 1000)
#define FUNCONF_DEBUG_HARDFAULT     0
#define FUNCONF_USE_CLK_SEC         0
#define FUNCONF_USE_5V_VDD          0

#endif
