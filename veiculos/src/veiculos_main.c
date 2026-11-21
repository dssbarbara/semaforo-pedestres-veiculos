/* ==== PLACA DOS VEICULOS (FRDM-KL25Z) ====
 * led0 = verde, led2 = vermelho, amarelo = led0 + led2 juntos
 * Fio de sincronismo: PTD4 (D2) -> sai daqui e vai para a placa dos pedestres
 * Nao esqueca de ligar o GND das duas placas.
 */
#if __has_include(<zephyr/kernel.h>)
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#else
#include <zephyr.h>
#include <drivers/gpio.h>
#endif

/* ---- Modos de teste de unidade (descomente no MAXIMO um) ---- */
/* #define TEST_ONLY_GREEN  */
/* #define TEST_ONLY_YELLOW */
/* #define TEST_ONLY_RED    */

#if defined(TEST_ONLY_GREEN) || defined(TEST_ONLY_YELLOW) || defined(TEST_ONLY_RED)
#define ISOLADO 1
#else
#define ISOLADO 0
#endif

static const struct gpio_dt_spec led_g = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_r = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
/* sinal "veiculos no vermelho": 1 enquanto o vermelho dos carros esta aceso */
static const struct gpio_dt_spec sync_out = {
	.port = DEVICE_DT_GET(DT_NODELABEL(gpiod)), .pin = 4, .dt_flags = GPIO_ACTIVE_HIGH };

#define STACK_SIZE  1024
#define THREAD_PRIO 5
#define GREEN_MS    3000
#define YELLOW_MS   1000
#define RED_MS      4000

enum color { C_GREEN, C_YELLOW, C_RED, C_NONE };

K_MUTEX_DEFINE(veh_mutex);
static volatile enum color turn = C_GREEN;  /* de quem e a vez */
static volatile enum color owner = C_NONE;  /* quem esta com os LEDs agora */

K_THREAD_STACK_DEFINE(green_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(yellow_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(red_stack, STACK_SIZE);
static struct k_thread green_data, yellow_data, red_data;

static void set_leds(enum color c)
{
	gpio_pin_set_dt(&led_g, c == C_GREEN || c == C_YELLOW);
	gpio_pin_set_dt(&led_r, c == C_RED || c == C_YELLOW);
}

static void green_thread(void *p1, void *p2, void *p3)
{
	while (1) {
		k_mutex_lock(&veh_mutex, K_FOREVER);
		if (turn == C_GREEN) {
			if (owner != C_NONE) printk("ERRO: exclusao mutua violada\n");
			owner = C_GREEN;
			set_leds(C_GREEN);
			printk("SINAL VERDE ON    t=%lld\n", k_uptime_get());
			k_msleep(GREEN_MS);
			set_leds(C_NONE);
			printk("SINAL VERDE OFF   t=%lld\n", k_uptime_get());
			owner = C_NONE;
			turn = ISOLADO ? C_GREEN : C_YELLOW;
		}
		k_mutex_unlock(&veh_mutex);
		k_msleep(5);
	}
}

static void yellow_thread(void *p1, void *p2, void *p3)
{
	while (1) {
		k_mutex_lock(&veh_mutex, K_FOREVER);
		if (turn == C_YELLOW) {
			if (owner != C_NONE) printk("ERRO: exclusao mutua violada\n");
			owner = C_YELLOW;
			set_leds(C_YELLOW);
			printk("SINAL AMARELO ON  t=%lld\n", k_uptime_get());
			k_msleep(YELLOW_MS);
			set_leds(C_NONE);
			printk("SINAL AMARELO OFF t=%lld\n", k_uptime_get());
			owner = C_NONE;
			turn = ISOLADO ? C_YELLOW : C_RED;
		}
		k_mutex_unlock(&veh_mutex);
		k_msleep(5);
	}
}

static void red_thread(void *p1, void *p2, void *p3)
{
	while (1) {
		k_mutex_lock(&veh_mutex, K_FOREVER);
		if (turn == C_RED) {
			if (owner != C_NONE) printk("ERRO: exclusao mutua violada\n");
			owner = C_RED;
			set_leds(C_RED);
			gpio_pin_set_dt(&sync_out, 1);      /* avisa os pedestres */
			printk("SINAL VERMELHO ON  t=%lld\n", k_uptime_get());
			k_msleep(RED_MS);
			gpio_pin_set_dt(&sync_out, 0);
			set_leds(C_NONE);
			printk("SINAL VERMELHO OFF t=%lld\n", k_uptime_get());
			owner = C_NONE;
			turn = ISOLADO ? C_RED : C_GREEN;
		}
		k_mutex_unlock(&veh_mutex);
		k_msleep(5);
	}
}

int main(void)
{
	if (!device_is_ready(led_g.port) || !device_is_ready(led_r.port) ||
	    !device_is_ready(sync_out.port)) {
		printk("ERRO: GPIO nao pronto\n");
		return 0;
	}
	gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&led_r, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&sync_out, GPIO_OUTPUT_INACTIVE);

#if defined(TEST_ONLY_YELLOW)
	turn = C_YELLOW;
#elif defined(TEST_ONLY_RED)
	turn = C_RED;
#endif

#if !defined(TEST_ONLY_YELLOW) && !defined(TEST_ONLY_RED)
	k_thread_create(&green_data, green_stack, STACK_SIZE, green_thread,
			NULL, NULL, NULL, THREAD_PRIO, 0, K_NO_WAIT);
#endif
#if !defined(TEST_ONLY_GREEN) && !defined(TEST_ONLY_RED)
	k_thread_create(&yellow_data, yellow_stack, STACK_SIZE, yellow_thread,
			NULL, NULL, NULL, THREAD_PRIO, 0, K_NO_WAIT);
#endif
#if !defined(TEST_ONLY_GREEN) && !defined(TEST_ONLY_YELLOW)
	k_thread_create(&red_data, red_stack, STACK_SIZE, red_thread,
			NULL, NULL, NULL, THREAD_PRIO, 0, K_NO_WAIT);
#endif
	return 0;
}