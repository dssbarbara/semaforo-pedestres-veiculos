#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* Na FRDM-KL25Z: led0 = verde (PTB19), led1 = vermelho (PTB18).*/
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_red   = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

#define STACK_SIZE   512
#define THREAD_PRIO  5
#define PED_TIME_MS  4000

K_MUTEX_DEFINE(ped_mutex);

K_THREAD_STACK_DEFINE(green_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(red_stack, STACK_SIZE);
static struct k_thread green_data;
static struct k_thread red_data;

static void green_thread(void *p1, void *p2, void *p3)
{
	while (1) {
		k_mutex_lock(&ped_mutex, K_FOREVER);
		gpio_pin_set_dt(&led_green, 1);
		k_msleep(PED_TIME_MS);
		gpio_pin_set_dt(&led_green, 0);
		k_mutex_unlock(&ped_mutex);
		k_yield();
	}
}

static void red_thread(void *p1, void *p2, void *p3)
{
	while (1) {
		k_mutex_lock(&ped_mutex, K_FOREVER);
		gpio_pin_set_dt(&led_red, 1);
		k_msleep(PED_TIME_MS);
		gpio_pin_set_dt(&led_red, 0);
		k_mutex_unlock(&ped_mutex);
		k_yield();
	}
}

int main(void)
{
	if (!gpio_is_ready_dt(&led_green) || !gpio_is_ready_dt(&led_red)) {
		return 0;
	}

	gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);

	/* Verde começa imediatamente */
	k_thread_create(&green_data, green_stack, STACK_SIZE,
			green_thread, NULL, NULL, NULL,
			THREAD_PRIO, 0, K_NO_WAIT);

	/* Vermelho começa 10 ms depois, já encontrando o mutex ocupado */
	k_thread_create(&red_data, red_stack, STACK_SIZE,
			red_thread, NULL, NULL, NULL,
			THREAD_PRIO, 0, K_MSEC(10));

	return 0;
}