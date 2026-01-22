#include <stdio.h>
#include "pico/stdlib.h"

int main() {
    stdio_init_all();
    
    // LED点滅用（オンボードLED）
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    printf("=== Pico 2 Test Started ===\n");
    
    int count = 0;
    while (true) {
        gpio_put(LED_PIN, 1);
        printf("LED ON - Count: %d\n", count++);
        sleep_ms(500);
        
        gpio_put(LED_PIN, 0);
        printf("LED OFF\n");
        sleep_ms(500);
    }
    
    return 0;
}
