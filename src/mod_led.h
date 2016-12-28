/**
 * @file    src/mod_led.h
 * @brief
 *
 * @addtogroup
 * @{
 */

#ifndef _MOD_LED_H_
#define _MOD_LED_H_

#include "hal.h"
#include "targetconf.h"

/*===========================================================================*/
/* Module constants.                                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Module pre-compile time settings.                                         */
/*===========================================================================*/


/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Module data structures and types.                                         */
/*===========================================================================*/


/**
 * @brief   Driver configuration structure.
 * @note    Implementations may extend this structure to contain more,
 *          architecture dependent, fields.
 */
typedef struct {
    GPIOTYPE *gpio;
    uint32_t pin;
    bool clearOn;
} ModLEDConfig;

/**
 * @brief   Structure representing an SPI driver.
 * @note    Implementations may extend this structure to contain more,
 *          architecture dependent, fields.
 */
typedef struct
{
   const ModLEDConfig           *config;
} ModLED;

/*===========================================================================*/
/* Module macros.                                                            */
/*===========================================================================*/

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif
  void mod_led_init(ModLED* ledp, ModLEDConfig* ledConfp);
  void mod_led_on(ModLED* ledp);
  void mod_led_off(ModLED* ledp);
  void mod_led_shutdown(ModLED* ledp);
#ifdef __cplusplus
}
#endif

#endif /* _MOD_LED_H_ */

/** @} */
