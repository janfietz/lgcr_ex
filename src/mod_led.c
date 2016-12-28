/**
 * @file    src/mod_led.c
 * @brief
 *
 * @addtogroup
 * @{
 */

#include "mod_led.h"


#if HAL_USE_PAL || defined(__DOXYGEN__)

#endif /* HAL_USE_PAL */

void mod_led_init(ModLED* ledp, ModLEDConfig* ledConfp)
{
    ledp->config = ledConfp;
    mod_led_off(ledp);
}

void mod_led_on(ModLED* ledp)
{
    if (ledp == NULL)
    {
        return;
    }

    if (ledp->config->clearOn == true)
    {
        palClearPad(ledp->config->gpio, ledp->config->pin);
    }
    else
    {
        palSetPad(ledp->config->gpio, ledp->config->pin);
    }
}

void mod_led_off(ModLED* ledp)
{
    if (ledp == NULL)
    {
        return;
    }

    if (ledp->config->clearOn == true)
    {
        palSetPad(ledp->config->gpio, ledp->config->pin);
    }
    else
    {
        palClearPad(ledp->config->gpio, ledp->config->pin);
    }
}

void mod_led_shutdown(ModLED* ledp)
{
    (void)ledp;
}

/** @} */
