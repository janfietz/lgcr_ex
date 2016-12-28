
#include "ch.h"
#include "hal.h"

/* addition driver */

#include "board_drivers.h"
#include "targetconf.h"

#include "chprintf.h"

#include <stdlib.h>

#include "mod_led.h"

ModLED LED_BMS_HEARTBEAT;
ModLED LED_CAN_RX;
ModLED LED_BOARDHEARTBEAT;



#define USE_WDG FALSE

//Declare Messagepool for at least 5 msg
static CANRxFrame CANRxFrameBuffer[10];
static MEMORYPOOL_DECL(canRXMessagesPool, sizeof(CANRxFrame), NULL);

//set up Mailbox
static msg_t canRXMailboxQueue[10];
static MAILBOX_DECL(canRX, canRXMailboxQueue, 10);

static THD_WORKING_AREA(mailboxProcessWa, 512);
static THD_FUNCTION(mailboxProcess, arg)
{

    (void) arg;
    char printBuffer[256];
    chRegSetThreadName("mailbox");
    size_t bytesWritten = 0;
    while (!chThdShouldTerminateX())
    {
        msg_t pbuf;

        /* Processing the event.*/
        while (chMBFetch(&canRX, (msg_t *) &pbuf, TIME_IMMEDIATE) == MSG_OK )
        {
            CANRxFrame* prxmsg = (CANRxFrame*) pbuf;

            /* Process message.*/
            int bytes = chsnprintf(printBuffer, sizeof(printBuffer),
                    "%08lx: %08lx %08lx\r\n", (uint32_t) prxmsg->SID,
                    (uint32_t)prxmsg->data32[0], (uint32_t)prxmsg->data32[1]);

            chPoolFree(&canRXMessagesPool, prxmsg);

            // write buffer to usb stream
            bytesWritten = chnWriteTimeout((BaseChannel* )&SERIALDRIVER, (uint8_t* )printBuffer, bytes,
                    MS2ST(10));
        }
        chThdSleepMilliseconds(100);
    }
}

/*
 * Can receiving threads signals this thread. This thread controls a LED
 * to show a received message.
 */
static thread_t *tpRXNotification;
static THD_WORKING_AREA(rx_notification_wa, 256);
static THD_FUNCTION(rx_notification, arg)
{

    (void) arg;
    chRegSetThreadName("rx_blink");

    tpRXNotification = chThdGetSelfX();
    mod_led_off(&LED_CAN_RX);
    while (!chThdShouldTerminateX())
    {
        if (chEvtWaitAnyTimeout((eventmask_t) 1, MS2ST(100)) == 0)
        {
            continue;
        }

        mod_led_on(&LED_CAN_RX);
        chThdSleepMilliseconds(200);
        mod_led_off(&LED_CAN_RX);
    }
}

/*
 * CAN receiver thread
 */
static THD_WORKING_AREA(can_rx_wa, 256);
static THD_FUNCTION(can_rx, arg)
{
    (void) arg;
    chRegSetThreadName("receiver");

    event_listener_t el;

    chEvtRegister(&CANDRIVER.rxfull_event, &el, 0);
    while (!chThdShouldTerminateX())
    {
        if (chEvtWaitAnyTimeout(ALL_EVENTS, MS2ST(100)) == 0)
            continue;
        CANRxFrame* prxmsg = (CANRxFrame*) chPoolAlloc(&canRXMessagesPool);
        while (canReceive(&CANDRIVER, CAN_ANY_MAILBOX, prxmsg, TIME_IMMEDIATE)
                == MSG_OK )
        {
            if (tpRXNotification != NULL)
            {
                chEvtSignal(tpRXNotification, (eventmask_t) 1);
            }
            if (chMBPost(&canRX, (msg_t) prxmsg, MS2ST(1)) == MSG_OK)
            {
                prxmsg = (CANRxFrame*) chPoolAlloc(&canRXMessagesPool);
            }
        }
        chPoolFree(&canRXMessagesPool, prxmsg);
    }
    chEvtUnregister(&CANDRIVER.rxfull_event, &el);
}

/*
 * This is a periodic thread that sends a heartbeat to BMS
 */
static THD_WORKING_AREA(can_tx_wa, 256);
static THD_FUNCTION(can_tx, arg)
{
    (void) arg;
    chRegSetThreadName("transmitter");

    CANTxFrame txmsg;
    txmsg.IDE = CAN_IDE_STD;
    txmsg.EID = 0x00000305;
    txmsg.RTR = CAN_RTR_DATA;
    txmsg.DLC = 8;
    txmsg.data32[0] = 0x00000000;
    txmsg.data32[1] = 0x00000000;

    mod_led_off(&LED_BMS_HEARTBEAT);

    while (!chThdShouldTerminateX())
    {
        if (canTransmit(&CANDRIVER, CAN_ANY_MAILBOX, &txmsg, MS2ST(100)) == MSG_OK)
        {
            //signal transmit with LED
            mod_led_on(&LED_BMS_HEARTBEAT);
            chThdSleepMilliseconds(200);

            mod_led_off(&LED_BMS_HEARTBEAT);

        }
        chThdSleepMilliseconds(20000);

   }
}

/*
 * This is a periodic thread that blinks a LED to show board activity
 */
static THD_WORKING_AREA(board_heartbeat_wa, 256);
static THD_FUNCTION(board_heartbeat, arg)
{
    (void) arg;
    chRegSetThreadName("heartbeat");

    mod_led_off(&LED_BOARDHEARTBEAT);

    bool ledOn = false;

    while (!chThdShouldTerminateX())
    {
        if (ledOn == true)
            mod_led_off(&LED_BOARDHEARTBEAT);
        else
            mod_led_on(&LED_BOARDHEARTBEAT);

        ledOn = !ledOn;

        chThdSleepSeconds(1);
    }
}


/*
 * Application entry point.
 */

int main(void)
{
    chPoolObjectInit(&canRXMessagesPool, sizeof(CANRxFrame), NULL);
    chPoolLoadArray(&canRXMessagesPool, CANRxFrameBuffer, 10);
    chMBObjectInit(&canRX, canRXMailboxQueue, 10);

    /*
     * System initializations.
     * - HAL initialization, this also initializes the configured device drivers
     *   and performs the board-specific initializations.
     * - Kernel initialization, the main() function becomes a thread and the
     *   RTOS is active.
     */
    halInit();

    chSysInit();


    BoardDriverInit();

    BoardDriverStart();

    /*
     * Creates threads.
     */
    chThdCreateStatic(rx_notification_wa, sizeof(rx_notification_wa),
            LOWPRIO + 1, rx_notification, NULL);

    chThdCreateStatic(can_rx_wa, sizeof(can_rx_wa), NORMALPRIO + 7, can_rx,
            NULL);

    chThdCreateStatic(can_tx_wa, sizeof(can_tx_wa), NORMALPRIO + 7, can_tx,
            NULL);

    chThdCreateStatic(mailboxProcessWa, sizeof(mailboxProcessWa), LOWPRIO,
            mailboxProcess, NULL);

    chThdCreateStatic(board_heartbeat_wa, sizeof(board_heartbeat_wa), LOWPRIO,
            board_heartbeat, NULL);

    while (TRUE)
    {
        chThdSleepMilliseconds(500);
    }
}
