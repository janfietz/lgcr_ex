/**
 * @file    board_drivers.c
 * @brief
 *
 * @{
 */

#include "board_drivers.h"

#include "ch.h"
#include "hal.h"

#include "usbcfg.h"

extern SerialUSBDriver SDU1;

/*
 * 500KBaud, automatic wakeup, automatic recover
 * from abort mode.
 * See section 22.7.7 on the STM32 reference manual.
 */
static const CANConfig cancfg = {
  CAN_MCR_ABOM | CAN_MCR_AWUM | CAN_MCR_TXFP,
  CAN_BTR_SJW(0) | CAN_BTR_TS2(1) |
  CAN_BTR_TS1(8) | CAN_BTR_BRP(6)
  //| CAN_BTR_LBKM
};

void BoardDriverInit(void)
{
    sduObjectInit(&SDU1);
}

void BoardDriverStart(void)
{
	canStart(&CAND1, &cancfg);
	palSetPadMode(GPIOD, 0, PAL_MODE_ALTERNATE(9));
	palSetPadMode(GPIOD, 1, PAL_STM32_OSPEED_HIGHEST | PAL_MODE_ALTERNATE(9));
    /*
     * Initializes a serial-over-USB CDC driver.
     */
    sduStart(&SDU1, &serusbcfg);

    /*
     * Activates the USB driver and then the USB bus pull-up on D+.
     * Note, a delay is inserted in order to not have to disconnect the cable
     * after a reset.
     */
    usbDisconnectBus(serusbcfg.usbp);
    chThdSleepMilliseconds(1000);
    usbStart(serusbcfg.usbp, &usbcfg);
    usbConnectBus(serusbcfg.usbp);
}

void BoardDriverShutdown(void)
{
    sduStop(&SDU1);
    canStop(&CAND1);
}

/** @} */
