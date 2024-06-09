#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include "led.h"
#include "value.h"

#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)
#define SW3_NODE DT_ALIAS(sw3)

#if !DT_NODE_HAS_STATUS(SW0_NODE, okay) || !DT_NODE_HAS_STATUS(SW1_NODE, okay) || !DT_NODE_HAS_STATUS(SW3_NODE, okay)
#error "Unsupported board: one or more button devicetree aliases are not defined"
#endif

static const struct gpio_dt_spec button0 = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET_OR(SW1_NODE, gpios, {0});
static const struct gpio_dt_spec button3 = GPIO_DT_SPEC_GET_OR(SW3_NODE, gpios, {0});

static struct gpio_callback button0_cb_data;
static struct gpio_callback button1_cb_data;
static struct gpio_callback button3_cb_data;

struct k_timer my_timer;
struct k_work my_work;

static int seconds = 0;
static bool increase_mode = true;
static bool timer_running = false;

void my_work_handler(struct k_work *work)
{
    printk("Time: %d\n", seconds);

    // Display the time on the LED matrix
    led_on_seconds(seconds);

    // Increment or decrement the seconds based on the mode
    if (increase_mode)
    {
        seconds++;
        if (seconds > 59)
        {
            seconds = 0;
        }
    }
    else
    {
        seconds--;
        if (seconds < 0)
        {
            seconds = 0;
        }
    }
}

K_WORK_DEFINE(my_work, my_work_handler);

void my_timer_handler(struct k_timer *dummy)
{
    if (timer_running)
    {
        k_work_submit(&my_work);
    }
}
K_TIMER_DEFINE(my_timer, my_timer_handler, NULL);

void button0_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    // Timer -> StopWatch
    // StopWatch -> Timer
    increase_mode = !increase_mode;
    if (increase_mode)
        seconds++;
    else
        seconds--;
    printk("Mode changed to %s\n", increase_mode ? "increase" : "decrease");
}

void button1_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    // 시간의 흐름을 중지하는 함수

    timer_running = !timer_running;
    printk("Timer %s\n", timer_running ? "started" : "stopped");
}

void button3_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    seconds = 0;
}

int configure_button(const struct gpio_dt_spec *button, struct gpio_callback *button_cb_data, gpio_callback_handler_t handler)
{
    int ret;

    if (!device_is_ready(button->port))
    {
        printk("Error: button device %s is not ready\n", button->port->name);
        return -1;
    }

    ret = gpio_pin_configure_dt(button, GPIO_INPUT);
    if (ret != 0)
    {
        printk("Error %d: failed to configure %s pin %d\n", ret, button->port->name, button->pin);
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(button, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0)
    {
        printk("Error %d: failed to configure interrupt on %s pin %d\n", ret, button->port->name, button->pin);
        return ret;
    }

    gpio_init_callback(button_cb_data, handler, BIT(button->pin));
    gpio_add_callback(button->port, button_cb_data);
    printk("Set up button at %s pin %d\n", button->port->name, button->pin);

    return 0;
}

int main(void)
{
    printk("Seconds counter example\n");
    int ret;

    ret = led_init();
    if (ret != DK_OK)
    {
        printk("Error initializing LED\n");
        return ret;
    }

    ret = configure_button(&button0, &button0_cb_data, button0_pressed);
    if (ret != 0)
    {
        return ret;
    }

    ret = configure_button(&button1, &button1_cb_data, button1_pressed);
    if (ret != 0)
    {
        return ret;
    }

    ret = configure_button(&button3, &button3_cb_data, button3_pressed);
    if (ret != 0)

        return ret;

    // Start the timer

    k_timer_start(&my_timer, K_SECONDS(1), K_SECONDS(1));

    return DK_OK;
}
