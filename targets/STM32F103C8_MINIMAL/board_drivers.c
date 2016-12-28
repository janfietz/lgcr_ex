/**
 * @file    board_drivers.c
 * @brief
 *
 * @{
 */

#include "board_drivers.h"

#include "ch.h"
#include "hal.h"

#include "mod_led.h"


extern ModLED LED_BMS_HEARTBEAT;
extern ModLED LED_CAN_RX;
extern ModLED LED_BOARDHEARTBEAT;

static ModLEDConfig ledCfg1 = {GPIOA, 5, false};
static ModLEDConfig ledCfg2 = {GPIOA, 6, false};
static ModLEDConfig ledCfg3 = {GPIOA, 4, false};

/** @brief Driver default configuration.*/
static const SerialConfig serialConfig =
{
  SERIAL_DEFAULT_BITRATE,
  0,
  USART_CR2_STOP1_BITS,
  0
};

/*
 * 500KBaud, automatic wakeup, automatic recover
 * from abort mode.
 * See section 22.7.7 on the STM32 reference manual.
 */
static const CANConfig cancfg = {
  CAN_MCR_ABOM | CAN_MCR_AWUM | CAN_MCR_TXFP,
  CAN_BTR_SJW(0) | CAN_BTR_TS2(1) |
  CAN_BTR_TS1(8) | CAN_BTR_BRP(4)
  //| CAN_BTR_LBKM
};

void BoardDriverInit(void)
{
    mod_led_init(&LED_BMS_HEARTBEAT, &ledCfg1);
    mod_led_init(&LED_CAN_RX, &ledCfg2);
    mod_led_init(&LED_BOARDHEARTBEAT, &ledCfg3);
}

void BoardDriverStart(void)
{
	canStart(&CAND1, &cancfg);

	/*CAN1 RX and TX*/
	palSetPadMode(GPIOB, 8, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
	palSetPadMode(GPIOB, 9, PAL_MODE_STM32_ALTERNATE_PUSHPULL);

	sdStart(&SD2, &serialConfig);
	palSetPadMode(GPIOA, 0, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
    palSetPadMode(GPIOA, 1, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
	palSetPadMode(GPIOA, 2, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
    palSetPadMode(GPIOA, 3, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
}

void BoardDriverShutdown(void)
{
    sdStop(&SD2);
    canStop(&CAND1);
}

/** @} */
