#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"

#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/bootrom.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "lib/func.c"

// definição dos LEDs
#define LUZ_VERMELHA 11

// configuração do joystick
#define EIXO_HORIZONTAL 26
#define EIXO_VERTICAL 27
// Configuração dos botões
const uint BOTAO_JOYSTICK = 22;
const uint botao_A = 5;
const uint botao_B = 6;

// debounce e atualização do LED
static volatile uint32_t tempo_ultimo_clique = 0;

// Configuração do pino do buzzer
#define BUZZER_PIN 21 // Configuração da frequência do buzzer (em Hz)
#define BUZZER_FREQUENCY 5000
bool buzzer_active = false; // Flag para controle do buzzer

// Teste botão do joystick
bool quadrado = false;

// Apertar Botão A para sair do loop
uint cont = 0;

// Variáveis limetes Joystick
uint limitsADC[4];

uint slice_num; // Variável PWM

// Definição de uma função para inicializar o PWM no pino do buzzer
void pwm_init_buzzer(uint pin)
{
    // Configurar o pino como saída de PWM
    gpio_set_function(pin, GPIO_FUNC_PWM);

    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o PWM com frequência desejada
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096)); // Divisor de clock
    pwm_init(slice_num, &config, true);

    // Iniciar o PWM no nível baixo
    pwm_set_gpio_level(pin, 0);
}

// Definição de uma função para emitir um beep com duração especificada
void beep(uint pin)
{
    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o duty cycle para 50% (ativo)
    pwm_set_gpio_level(pin, 2048);

    // Temporização
    sleep_ms(300);

    // Desativar o sinal PWM (duty cycle 0)
    pwm_set_gpio_level(pin, 0);
}

void handler_interrupcao(uint gpio, uint32_t eventos)
{
    uint32_t tempo_atual = to_us_since_boot(get_absolute_time());

    if (gpio == botao_B && tempo_atual - tempo_ultimo_clique > 200000)
    {
        tempo_ultimo_clique = tempo_atual;
        if (cont > 0)
        {
            cont--;
        }
    }
    else if (gpio == BOTAO_JOYSTICK)
    {
        if (tempo_atual - tempo_ultimo_clique > 200000)
        { // debounce de 200ms
            tempo_ultimo_clique = tempo_atual;
            quadrado = !quadrado;
        }
    }
    else if (gpio == botao_A)
    {
        if (tempo_atual - tempo_ultimo_clique > 200000)
        { // debounce de 200ms
            tempo_ultimo_clique = tempo_atual;
            limitsADC[cont] = adc_read();
            if (cont < 5)
            {
                cont++;
            }
            buzzer_active = true;
        }
    }
}

// calcula a posição horizontal do joystick
int PosiX(int ADC_value, uint max, uint min)
{
    int resultado;
    float fator = 0.0;
    fator = (float)ADC_value / ((max - min) / 2) / 4;
    resultado = (fator * 60) + 45;

    return resultado;
}

// calcula a posição vertical do joystick
int PosiY(int ADC_value, uint max, uint min)
{
    int resultado, intensidade;
    float fator = 0.0;
    fator = (float)ADC_value / ((max - min) / 2) / 2;
    resultado = 45 - (fator * 30);

    return resultado;
}

int main()
{
    stdio_init_all();
    adc_init();

    // inicialização do display OLED via I2C
    ssd1306_t tela;
    ssd1306_conf_init(&tela);

    // Butão e IRQ
    initButton(botao_A);
    initButton(botao_B);
    gpio_set_irq_enabled_with_callback(botao_A, GPIO_IRQ_EDGE_FALL, true, &handler_interrupcao);
    gpio_set_irq_enabled_with_callback(botao_B, GPIO_IRQ_EDGE_FALL, true, &handler_interrupcao);

    // configuração do joystick
    initButton(BOTAO_JOYSTICK);
    gpio_set_irq_enabled_with_callback(BOTAO_JOYSTICK, GPIO_IRQ_EDGE_FALL, true, &handler_interrupcao);

    adc_gpio_init(EIXO_HORIZONTAL);
    adc_gpio_init(EIXO_VERTICAL);

    uint16_t valor_adc_x;
    uint16_t valor_adc_y;
    char buffer_xmax[5];
    char buffer_ymax[5];
    char buffer_xmin[5];
    char buffer_ymin[5];

    int posX, posY;
    bool inverter_cor = true;
    // Configuração do GPIO para o buzzer como saída
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    // Inicializar o PWM no pino do buzzer
    pwm_init_buzzer(BUZZER_PIN);

    while (true)
    {
        if (cont == 0)
        {
            telaEsquerda(&tela);
            adc_select_input(1);
        }
        else if (cont == 1)
        {
            telaDireita(&tela);
            adc_select_input(1);
        }
        else if (cont == 2)
        {
            adc_select_input(0);
            telaCima(&tela);
        }
        else if (cont == 3)
        {
            adc_select_input(0);
            telaBaixo(&tela);
        }
        else if (cont == 4)
        {
            sprintf(buffer_xmax, "%d", limitsADC[1]);
            sprintf(buffer_ymax, "%d", limitsADC[2]);
            sprintf(buffer_xmin, "%d", limitsADC[0]);
            sprintf(buffer_ymin, "%d", limitsADC[3]);
            valores(&tela, buffer_xmax, buffer_ymax, buffer_xmin, buffer_ymin);
        }
        else
        {
            adc_select_input(1);
            valor_adc_x = adc_read();
            adc_select_input(0);
            valor_adc_y = adc_read();

            posX = PosiX(valor_adc_x, limitsADC[1], limitsADC[0]);
            posY = PosiY(valor_adc_y, limitsADC[2], limitsADC[3]);

            ssd1306_fill(&tela, !inverter_cor);
            ssd1306_rect(&tela, 10, 39, 45, 45, inverter_cor, !inverter_cor);
            ssd1306_hline(&tela, 31, 95, 31, true); // linha horizontal
            ssd1306_vline(&tela, 62, 5, 60, true);  // linha vertical
            ssd1306_rect(&tela, posY, posX, 5, 5, inverter_cor, quadrado);
            ssd1306_send_data(&tela);
        }
        if(buzzer_active){
            beep(BUZZER_PIN);
            buzzer_active = false;
        }
    }
}
