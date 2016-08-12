/*
 ChibiOS - Copyright (C) 2006-2014 Giovanni Di Sirio

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

#include "ch.h"
#include "hal.h"

/* addition driver */

#include "board_drivers.h"

#include "chprintf.h"
#include "shell.h"

#include "usbcfg.h"
#include <stdlib.h>

#define LED_BMS_HEARTBEAT GPIOD_LED3 //orange
#define LED_CAN_RX GPIOD_LED4 //green
#define LED_RED GPIOD_LED5 //red
#define LED_BOARDHEARTBEAT GPIOD_LED6 //blue

#define LED_ON(LED) palSetPad(GPIOD, LED)
#define LED_OFF(LED) palClearPad(GPIOD, LED)
#define LED_TOGGLE(LED) palTogglePad(GPIOD, LED)


#define USE_WDG FALSE

struct can_instance {
  CANDriver     *canp;
  uint32_t      led;
};

static const struct can_instance can1 = {&CAND1, LED_CAN_RX};


/* Virtual serial port over USB.*/
#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(2048)

static void cmd_mem(BaseSequentialStream *chp, int argc, char *argv[]) {
  size_t n, size;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: mem\r\n");
    return;
  }
  n = chHeapStatus(NULL, &size);
  chprintf(chp, "core free memory : %u bytes\r\n", chCoreGetStatusX());
  chprintf(chp, "heap fragments   : %u\r\n", n);
  chprintf(chp, "heap free total  : %u bytes\r\n", size);
}

static void cmd_threads(BaseSequentialStream *chp, int argc, char *argv[]) {
  static const char *states[] = {CH_STATE_NAMES};
  thread_t *tp;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: threads\r\n");
    return;
  }
  chprintf(chp, "    addr    stack prio refs     state\r\n");
  tp = chRegFirstThread();
  do {
    chprintf(chp, "%08lx %08lx %4lu %4lu %9s\r\n",
             (uint32_t)tp, (uint32_t)tp->p_ctx.r13,
             (uint32_t)tp->p_prio, (uint32_t)(tp->p_refs - 1),
             states[tp->p_state]);
    tp = chRegNextThread(tp);
  } while (tp != NULL);
}

static const ShellCommand commands[] = {
  {"mem", cmd_mem},
  {"threads", cmd_threads},
  {NULL, NULL}
};

static const ShellConfig shell_cfg1 = {
  (BaseSequentialStream *)&SDU1,
  commands
};

static thread_t *tpRXNotification;
static THD_WORKING_AREA(rx_notification_wa, 256);
static THD_FUNCTION(rx_notification, p) {

    (void)p;
    chRegSetThreadName("rx_blink");
    tpRXNotification = chThdGetSelfX();
    LED_OFF(LED_CAN_RX);
    while (!chThdShouldTerminateX())
    {
         if (chEvtWaitAnyTimeout((eventmask_t)1, MS2ST(100)) == 0)
         {
             continue;
         }

         LED_ON(LED_CAN_RX);
         chThdSleepMilliseconds(200);
         LED_OFF(LED_CAN_RX);
    }
}


/*
 * CAN receiver thread
 */
static THD_WORKING_AREA(can_rx_wa, 256);
static THD_FUNCTION(can_rx, p) {
  struct can_instance *cip = p;
  event_listener_t el;
  CANRxFrame rxmsg;

  (void)p;


  chRegSetThreadName("receiver");
  chEvtRegister(&cip->canp->rxfull_event, &el, 0);
  while(!chThdShouldTerminateX()) {
    if (chEvtWaitAnyTimeout(ALL_EVENTS, MS2ST(100)) == 0)
      continue;
    while (canReceive(cip->canp, CAN_ANY_MAILBOX,
                      &rxmsg, TIME_IMMEDIATE) == MSG_OK) {
      chEvtSignal(tpRXNotification, (eventmask_t)1);
      /* Process message.*/
//      if (SDU1.config->usbp->state == USB_ACTIVE)
//      {
//          chprintf((BaseSequentialStream *)&SDU1, "%08u: %08lx %08lx\r\n",
//                             (uint32_t)rxmsg.SID, (uint32_t)rxmsg.data32[0], (uint32_t)rxmsg.data32[1]);
//      }

    }
  }
  chEvtUnregister(&cip->canp->rxfull_event, &el);
}

/*
 * This is a periodic thread that sends a heartbeat to BMS
 */
static THD_WORKING_AREA(can_tx_wa, 256);
static THD_FUNCTION(can_tx, arg) {

	CANTxFrame txmsg;

	(void)arg;
	chRegSetThreadName("transmitter");
	txmsg.IDE = CAN_IDE_STD;
	txmsg.EID = 0x00000131;
	txmsg.RTR = CAN_RTR_DATA;
	txmsg.DLC = 8;
	txmsg.data32[0] = 0x00000000;
	txmsg.data32[1] = 0x00000000;

	LED_OFF(LED_BMS_HEARTBEAT);

	while (!chThdShouldTerminateX()) {
		canTransmit(&CAND1, CAN_ANY_MAILBOX, &txmsg, MS2ST(100));

		//signal transmit with LED
		LED_ON(LED_BMS_HEARTBEAT);
		chThdSleepMilliseconds(200);
		LED_OFF(LED_BMS_HEARTBEAT);
		chThdSleepMilliseconds(20000);
	}
}

/*
 * This is a periodic thread that blinks a LED to show board activity
 */
static THD_WORKING_AREA(board_heartbeat_wa, 256);
static THD_FUNCTION(board_heartbeat, arg) {

    (void)arg;
    chRegSetThreadName("heartbeat");

    LED_OFF(LED_BOARDHEARTBEAT);

    while (!chThdShouldTerminateX())
    {
        LED_TOGGLE(LED_BOARDHEARTBEAT);
        chThdSleepSeconds(1);
    }
}


/*
 * Application entry point.
 */

int main(void)
{
    //thread_t *shelltp = NULL;

    /*
     * System initializations.
     * - HAL initialization, this also initializes the configured device drivers
     *   and performs the board-specific initializations.
     * - Kernel initialization, the main() function becomes a thread and the
     *   RTOS is active.
     */
    halInit();

    chSysInit();

    /*
     * Shell manager initialization.
     */
    shellInit();

    BoardDriverInit();

    BoardDriverStart();


    /*
    * Creates threads.
    */
    chThdCreateStatic(rx_notification_wa, sizeof(rx_notification_wa), LOWPRIO + 1,
            rx_notification, NULL);

    chThdCreateStatic(can_rx_wa, sizeof(can_rx_wa), NORMALPRIO + 7,
                      can_rx, (void *)&can1);
    chThdCreateStatic(can_tx_wa, sizeof(can_tx_wa), NORMALPRIO + 7,
                        can_tx, NULL);


    chThdCreateStatic(board_heartbeat_wa, sizeof(board_heartbeat_wa), LOWPRIO,
            board_heartbeat, NULL);

    /*
     * Normal main() thread activity, in this demo it just performs
     * a shell respawn upon its termination.
     */
    while (TRUE)
    {
//        if (!shelltp) {
//          if (SDU1.config->usbp->state == USB_ACTIVE) {
//            /* Spawns a new shell.*/
//            shelltp = shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
//          }
//        }
//        else {
//          /* If the previous shell exited.*/
//          if (chThdTerminatedX(shelltp)) {
//            /* Recovers memory of the previous shell.*/
//            chThdRelease(shelltp);
//            shelltp = NULL;
//          }
//        }
        chThdSleepMilliseconds(500);
    }
}
