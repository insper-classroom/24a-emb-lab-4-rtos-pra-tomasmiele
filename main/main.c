/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

#include "pico/util/datetime.h"
//#include "hardware/rtc.h"

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

const uint TRIG_PIN = 18;
const uint ECHO_PIN = 17;

volatile bool echo_received = false;
const uint MAX_DISTANCE = 100; // Máxima distância em cm que queremos medir

// Filas e semáforos para sincronização
QueueHandle_t xQueueDistance;
SemaphoreHandle_t xSemaphoreTrigger;
QueueHandle_t xQueueDistanceDisplay;
SemaphoreHandle_t xSemaphoreTimerFired;


void oled1_btn_led_init(void) {
    gpio_init(LED_1_OLED);
    gpio_set_dir(LED_1_OLED, GPIO_OUT);

    gpio_init(LED_2_OLED);
    gpio_set_dir(LED_2_OLED, GPIO_OUT);

    gpio_init(LED_3_OLED);
    gpio_set_dir(LED_3_OLED, GPIO_OUT);

    gpio_init(BTN_1_OLED);
    gpio_set_dir(BTN_1_OLED, GPIO_IN);
    gpio_pull_up(BTN_1_OLED);

    gpio_init(BTN_2_OLED);
    gpio_set_dir(BTN_2_OLED, GPIO_IN);
    gpio_pull_up(BTN_2_OLED);

    gpio_init(BTN_3_OLED);
    gpio_set_dir(BTN_3_OLED, GPIO_IN);
    gpio_pull_up(BTN_3_OLED);
}

void oled1_demo_1(void *p) {
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();

    char cnt = 15;
    while (1) {

        if (gpio_get(BTN_1_OLED) == 0) {
            cnt = 15;
            gpio_put(LED_1_OLED, 0);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "LED 1 - ON");
            gfx_show(&disp);
        } else if (gpio_get(BTN_2_OLED) == 0) {
            cnt = 15;
            gpio_put(LED_2_OLED, 0);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "LED 2 - ON");
            gfx_show(&disp);
        } else if (gpio_get(BTN_3_OLED) == 0) {
            cnt = 15;
            gpio_put(LED_3_OLED, 0);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "LED 3 - ON");
            gfx_show(&disp);
        } else {

            gpio_put(LED_1_OLED, 1);
            gpio_put(LED_2_OLED, 1);
            gpio_put(LED_3_OLED, 1);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "PRESSIONE ALGUM");
            gfx_draw_string(&disp, 0, 10, 1, "BOTAO");
            gfx_draw_line(&disp, 15, 27, cnt,
                          27);
            vTaskDelay(pdMS_TO_TICKS(50));
            if (++cnt == 112)
                cnt = 15;

            gfx_show(&disp);
        }
    }
}

void oled1_demo_2(void *p) {
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();

    while (1) {

        gfx_clear_buffer(&disp);
        gfx_draw_string(&disp, 0, 0, 1, "Mandioca");
        gfx_show(&disp);
        vTaskDelay(pdMS_TO_TICKS(150));

        gfx_clear_buffer(&disp);
        gfx_draw_string(&disp, 0, 0, 2, "Batata");
        gfx_show(&disp);
        vTaskDelay(pdMS_TO_TICKS(150));

        gfx_clear_buffer(&disp);
        gfx_draw_string(&disp, 0, 0, 4, "Inhame");
        gfx_show(&disp);
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

void gpio_callback(uint gpio, uint32_t events) {
    static uint64_t start_time = 0;
    uint64_t time_diff, end_time;
    uint64_t time_diff;

    if (events & GPIO_IRQ_EDGE_RISE) {
        start_time = to_us_since_boot(get_absolute_time());
    } else if (events & GPIO_IRQ_EDGE_FALL) {
        end_time = to_us_since_boot(get_absolute_time());
        time_diff = end_time - start_time;
        // Enviar somente se o tempo de resposta faz sentido
        if (time_diff < MAX_DISTANCE * 58) { // 58 us por cm é uma aproximação do tempo de viagem do som
            xQueueSendFromISR(xQueueDistance, &time_diff, NULL);
        } else {
            // Valor muito alto, pode ser um erro ou objeto muito distante
            time_diff = UINT64_MAX;
            xQueueSendFromISR(xQueueDistance, &time_diff, NULL);
        }
    }
}

void send_pulse() {
    gpio_put(TRIG_PIN, 1);
    sleep_us(10);
    gpio_put(TRIG_PIN, 0);
}

int64_t alarm_callback(alarm_id_t id, void *user_data) {
    xSemaphoreGiveFromISR(xSemaphoreTimerFired, NULL);
    return 1000; 
}

void sensor_init(void) {

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_put(TRIG_PIN, 0);

    add_alarm_in_ms(1000, alarm_callback, NULL, true);
}

void trigger_task(void *pvParameters) {
    while (1) {
        send_pulse();
        xSemaphoreGive(xSemaphoreTrigger);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void display_update(ssd1306_t *disp, float distance) {
    char buffer[32];
    gfx_clear_buffer(disp);

    if (distance == -1.0f) {
        snprintf(buffer, sizeof(buffer), "Sensor falhou");
    } else {
        snprintf(buffer, sizeof(buffer), "Dist: %.2f cm", distance);
    }

    gfx_draw_string(disp, 0, 0, 1, buffer);
    
    if (distance >= 0) {
        int bar_length = (int)((distance / MAX_DISTANCE) * (128 - 1));
        gfx_draw_line(disp, 0, 32 - 10, bar_length, 32 - 10);
    }

    gfx_show(disp);
}

void echo_task(void *pvParameters) {
    uint64_t time_elapsed;
    float distance;
    const TickType_t xTicksToWait = pdMS_TO_TICKS(1000);

    while (1) {
        if (xQueueReceive(xQueueDistance, &time_elapsed, xTicksToWait) == pdPASS) {
            distance = (time_elapsed == UINT64_MAX) ? UINT64_MAX : (float)time_elapsed * 0.0343 / 2.0;
        } else {
            // Timeout ocorreu, sensor falhou ou desconectado
            distance = -1;
        }
        xQueueSend(xQueueDistanceDisplay, &distance, portMAX_DELAY);
    }
}

void oled_task(void *pvParameters) {
    float distance;
    ssd1306_init();
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    while (1) {
        if (xQueueReceive(xQueueDistanceDisplay, &distance, portMAX_DELAY)) {
            if (distance == -1) {
                gfx_clear_buffer(&disp);
                gfx_draw_string(&disp, 0, 0, 1, "Sensor falhou");
                gfx_show(&disp);
            } else {
                display_update(&disp, distance);
            }
        }
    }
}


int main() {
    stdio_init_all();

    sensor_init();

    xQueueDistance = xQueueCreate(10, sizeof(uint64_t));
    xQueueDistanceDisplay = xQueueCreate(10, sizeof(float));
    xSemaphoreTrigger = xSemaphoreCreateBinary();
    xSemaphoreTimerFired = xSemaphoreCreateBinary();

    //xTaskCreate(oled1_demo_1, "Demo 1", 4095, NULL, 1, NULL);
    //xTaskCreate(oled1_demo_2, "Demo 2", 4095, NULL, 1, NULL);
    xTaskCreate(trigger_task, "Trigger Task", 256, NULL, 1, NULL);
    xTaskCreate(trigger_task, "Trigger Task", 256, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo Task", 256, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED Task", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
