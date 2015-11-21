/**
 * @file gpio.c GPIO BeagleBone Black Application module
 *
 * Copyright (C) 2015 Studio-Link.de
 */
#include <re.h>
#include <baresip.h>
#include <sys/time.h>
#include "event_gpio.h"

#define GPIO_LED 61
#define GPIO_BUTTON 66

static struct tmr tmr_alert;          /**< Incoming call alert timer      */
static int led_status = 0;
static struct ua *current_agent;
unsigned long long last_bounce = 0;


static void led_on(void) 
{
	led_status = 1;
	gpio_set_value(GPIO_LED, 1);
}


static void led_off(void) 
{
	led_status = 0;
	gpio_set_value(GPIO_LED, 0);
}

static void button_action(unsigned int gpio)
{
	info("BUTTON %d pressed \n", gpio);
	ua_answer(current_agent, NULL);
}


static void button_callback(unsigned int gpio)
{
	unsigned int bouncetime = 200; //milliseconds
	struct timeval tv_timenow;
	unsigned long long timenow = 0;

	gettimeofday(&tv_timenow, NULL);
	timenow = tv_timenow.tv_sec*1E6 + tv_timenow.tv_usec;

	if (timenow - last_bounce > bouncetime*1000) 
	{
		button_action(gpio);
	}
	last_bounce = timenow;
}


static void button_init(void)
{
	gpio_export(GPIO_BUTTON);
	sys_msleep(100); //Workaround: wait until udev sets permissions
	gpio_set_direction(GPIO_BUTTON, INPUT);
	add_edge_detect(GPIO_BUTTON, FALLING_EDGE);
	add_edge_callback(GPIO_BUTTON, &button_callback);
}

static void button_close(void) 
{
	remove_edge_detect(GPIO_BUTTON);
	gpio_unexport(GPIO_BUTTON);
}


static void led_init(void) 
{
	gpio_export(GPIO_LED);
	sys_msleep(100); //Workaround: wait until udev sets permissions
	gpio_set_direction(GPIO_LED, OUTPUT);
	led_off();
}


static void led_close(void) 
{
	led_off();
	gpio_unexport(GPIO_LED);
}


static void alert_start(void *arg)
{
	(void)arg;

	if (led_status) {
		led_off();
	} else {
		led_on();
	}
	
	tmr_start(&tmr_alert, 500, alert_start, NULL);
}


static void alert_stop(void)
{
	if (led_status) {
		led_off();
	}
	tmr_cancel(&tmr_alert);
}


static void ua_event_handler(struct ua *ua, enum ua_event ev,
		struct call *call, const char *prm, void *arg)
{
	(void)call;
	(void)prm;
	(void)arg;

	switch (ev) {
		case UA_EVENT_CALL_INCOMING:
			current_agent = ua;
			alert_start(0);
			break;

		case UA_EVENT_CALL_ESTABLISHED:
			alert_stop();
			break;

		case UA_EVENT_CALL_CLOSED:
			alert_stop();
			break;

		default:
			break;
	}
}


static int module_init(void)
{
	int err;

	tmr_init(&tmr_alert);
	led_init();
	button_init();
	err = uag_event_register(ua_event_handler, NULL);

	return err;
}


static int module_close(void)
{
	tmr_cancel(&tmr_alert);
	led_close();
	button_close();
	uag_event_unregister(ua_event_handler);

	return 0;
}


const struct mod_export DECL_EXPORTS(gpio) = {
	"gpio",
	"application",
	module_init,
	module_close
};
