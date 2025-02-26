#include <stdio.h>
#include "pico/stdlib.h"

void initButton(uint botao) {
    gpio_init(botao);
    gpio_set_dir(botao, GPIO_IN);
    gpio_pull_up(botao);
}

