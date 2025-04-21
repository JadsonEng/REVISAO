#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include <hardware/pio.h>
#include "inc/ssd1306.h"
#include "inc/font.h"

// Biblioteca para RESET USB
#include "pico/bootrom.h"

//  Biblioteca para animação na matriz de LED
#include "animacao_matriz.pio.h" // Biblioteca PIO para controle de LEDs WS2818B
#include "guitarhero.h"

// Configurando LEDs, Botões e Buzzer
#define LED_G 11
#define LED_B 12
#define LED_R 13
#define BTN_A 5
#define BTN_B 6
#define BTN_JOY 22
#define BUZZER1 21
#define LED_COUNT 25
#define LED_PIN 7

// Configurando Joystick
#define EIXO_Y 26    // ADC0
#define EIXO_X 27    // ADC1
#define PWM_WRAP 4095

// Configurando I2C
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15

// Configurando Display
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define SQUARE_SIZE 8

// Configurando posições do quadrado no display
int pos_x = (DISPLAY_WIDTH - SQUARE_SIZE) / 2;
int pos_y = (DISPLAY_HEIGHT - SQUARE_SIZE) / 2;
const int SPEED = 2;
const int MAX_X = DISPLAY_WIDTH - SQUARE_SIZE;
const int MAX_Y = DISPLAY_HEIGHT - SQUARE_SIZE;

// Declarando variáveis globais
volatile bool pwm_on = true;
volatile bool borda = false;
volatile bool led_g_estado = false;
bool cor = true;
absolute_time_t last_interrupt_time = 0;
volatile bool flagA = false;
volatile bool flagB = false;
volatile bool flagJ = false;

// Protótipos de funções
void gpio_callback(uint gpio, uint32_t events);
void JOYSTICK(uint slice1, uint slice2);
void mostraDetalhes();

// Algumas configurações para a matriz de LED
#define FPS 3                   // Taxa de quadros por segundo
#define LED_COUNT 25            // Número de LEDs na matriz
#define LED_PIN 7               // Pino GPIO conectado aos LEDs
int frame_delay = 1000 / FPS;   // Intervalo em milissegundos
int ordem[] = {0, 1, 2, 3, 4, 9, 8,7, 6, 5, 10, 11, 12, 13, 14, 19, 18, 17, 16, 15 , 20, 21, 22, 23, 24};  // Ordem da matriz de leds

void imprimir_binario(int num) {    //imprimir valor binário
 int i;
 for (i = 31; i >= 0; i--) {
  (num & (1 << i)) ? printf("1") : printf("0");
 }
}

//rotina para definição da intensidade de cores do led
uint32_t matrix_rgb(double b, double r, double g)   
{
  unsigned char R, G, B;
  R = r * 255;
  G = g * 255;
  B = b * 255;
  return (G << 24) | (R << 16) | (B << 8);
}

void init_pwm(uint gpio) {
    gpio_set_function(gpio, GPIO_FUNC_PWM); // Configura o GPIO como PWM
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_set_clkdiv(slice_num, 125.0f);     // Define o divisor do clock para 1 MHz
    pwm_set_wrap(slice_num, 1000);        // Define o TOP para frequência de 1 kHz
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(gpio), 0); // Razão cíclica inicial
    pwm_set_enabled(slice_num, true);     // Habilita o PWM
}

void set_buzzer_tone(uint gpio, uint freq) {
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    uint top = 1000000 / freq;            // Calcula o TOP para a frequência desejada
    pwm_set_wrap(slice_num, top);
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(gpio), top / 2); // 50% duty cycle
}

void stop_buzzer(uint gpio) {
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(gpio), 0); // Desliga o PWM
}


// Função para acender todos os LEDs com uma cor específica e intensidade em ponto flutuante
void acende_matrizLEDS(bool r, bool g, bool b, double intensidade, PIO pio, uint sm)
{
    double R = r * intensidade;
    double G = g * intensidade;
    double B = b * intensidade;
    for (uint i = 0; i < LED_COUNT; ++i){
        pio_sm_put_blocking(pio, sm, matrix_rgb(B, R, G));
    }
}

// Função da animação
void guitar_pio(int guitar[][25], PIO pio, uint sm) {
    uint32_t valor_led;
    for (int16_t k = 0; k < 15; k++) {
        for (int i = 0; i < LED_COUNT; i++) {
                switch (guitar[k][ordem[24-i]]) {
                    case 0: 
                        valor_led = matrix_rgb(0.0, 0.0, 0.0); 
                        break;
                    case 1: 
                        valor_led = matrix_rgb(0.75, 0.0, 0.75); //amarelo
                        break;
                    case 2:
                        valor_led = matrix_rgb(0.0, 0.75, 0.75); //laranja
                        break;
                    case 3:
                        valor_led = matrix_rgb(0.0, 0.0, 0.75); //verde
                        break;
                    case 4:
                        valor_led = matrix_rgb(0.5, 0.5, 0.5); //branco
                        break;
                    case 5:
                        valor_led = matrix_rgb(0, 0.75, 0.0); //vermelho
                        break;
                    case 6:
                        valor_led = matrix_rgb(0.75, 0.0, 0.0); //azul
                        break;
                    case 7:
                        valor_led = matrix_rgb(0.75, 0.0, 0.75); //ciano
                        break;
                    case 8:
                    valor_led = matrix_rgb(0.75, 0.75, 0.0); //roxo
                    break;
                    
                }

                pio_sm_put_blocking(pio, sm, valor_led);

        }
        imprimir_binario(valor_led);

        if(k == 5){
            set_buzzer_tone(BUZZER1, 330);
            sleep_ms(100);
            sleep_ms(frame_delay - 100);
        } else if(k == 6){
            set_buzzer_tone(BUZZER1, 330);
            sleep_ms(100);
            sleep_ms(frame_delay - 100);
        } else if(k == 7){
            set_buzzer_tone(BUZZER1, 260);
            sleep_ms(100);
            sleep_ms(frame_delay - 100);
        } else if(k == 8){
            set_buzzer_tone(BUZZER1, 260);
            sleep_ms(100);
            sleep_ms(frame_delay - 100);
        } else if(k == 9){
            set_buzzer_tone(BUZZER1, 290);
            sleep_ms(100);
            sleep_ms(frame_delay - 100);
        } else if(k == 10){
            set_buzzer_tone(BUZZER1, 290);
            sleep_ms(100);
            sleep_ms(frame_delay - 100);
        } else if(k == 11){
            set_buzzer_tone(BUZZER1, 330);
            sleep_ms(100);
            sleep_ms(frame_delay - 100);
        } else if(k == 12){
            set_buzzer_tone(BUZZER1, 260);
            sleep_ms(100);
            sleep_ms(frame_delay - 100);
        } else if(k == 13){
            sleep_ms(100);
            sleep_ms(frame_delay - 100);
        }
        else {
            stop_buzzer(BUZZER1);
            sleep_ms(100);             
            sleep_ms(frame_delay - 100);
        }
    }
}

bool cursor_x = false;

int main()
{
    stdio_init_all();

    // Iniciando e configurando os LEDs
    gpio_set_function(LED_B, GPIO_FUNC_PWM);
    gpio_set_function(LED_R, GPIO_FUNC_PWM);
    gpio_init(LED_G);
    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_put(LED_G, 0);

    // Iniciando e configurando os botões
    gpio_init(BTN_A);
    gpio_init(BTN_B);
    gpio_init(BTN_JOY);
    gpio_set_dir(BTN_A, GPIO_IN);
    gpio_set_dir(BTN_B, GPIO_IN);
    gpio_set_dir(BTN_JOY, GPIO_IN);
    gpio_pull_up(BTN_A);
    gpio_pull_up(BTN_B);
    gpio_pull_up(BTN_JOY);

    // Iniciando e configurando Buzzer
    gpio_init(BUZZER1);
    gpio_set_dir(BUZZER1, GPIO_OUT);
    init_pwm(BUZZER1);

    //  Habilitando Interrupção
    gpio_set_irq_enabled(BTN_A, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BTN_B, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BTN_JOY, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_callback(gpio_callback);
    irq_set_enabled(IO_IRQ_BANK0, true);
    
    //  Iniciando I2C
    i2c_init(I2C_PORT, 400*5000);    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    //  Iniciando ADC
    adc_init();
    adc_gpio_init(EIXO_Y);
    adc_gpio_init(EIXO_X);

    //  Iniciando PWM
    uint slice1 = pwm_gpio_to_slice_num(LED_B);
    uint slice2 = pwm_gpio_to_slice_num(LED_R);
    pwm_set_wrap(slice1, PWM_WRAP);
    pwm_set_wrap(slice2, PWM_WRAP);
    pwm_set_clkdiv(slice1, 2.0f);
    pwm_set_clkdiv(slice2, 2.0f);
    pwm_set_enabled(slice1, true);
    pwm_set_enabled(slice2, true);

    // Iniciando e configurando o Display
    ssd1306_t ssd;
    ssd1306_init(&ssd, DISPLAY_WIDTH, DISPLAY_HEIGHT, false, 0x3c, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Configurando PIO
    PIO pio = pio0; 
    bool ok;

    ok = set_sys_clock_khz(128000, false);      //coloca a frequência de clock para 128 MHz, facilitando a divisão pelo clock
    printf("iniciando a transmissão PIO");
    if (ok) printf("clock set to %ld\n", clock_get_hz(clk_sys));

    uint offset = pio_add_program(pio, &animacao_matriz_program);
    uint sm = pio_claim_unused_sm(pio, true);
    animacao_matriz_program_init(pio, sm, offset, LED_PIN);

    guitar_pio(guitar, pio, sm);
 
    while (true) {
        JOYSTICK(slice1, slice2);
        cor = !cor;

        // Desenha tudo
        ssd1306_fill(&ssd, false);
        if (borda) {
            ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);
        } else {
            ssd1306_rect(&ssd, 3, 3, 122, 58, true, false);
        }

        // cursor: quadrado ou X
        if (cursor_x) {
            ssd1306_line(&ssd, pos_x, pos_y,
                         pos_x + SQUARE_SIZE, pos_y + SQUARE_SIZE,
                         true);
            ssd1306_line(&ssd, pos_x, pos_y + SQUARE_SIZE,
                         pos_x + SQUARE_SIZE, pos_y,
                         true);
        } else {
            ssd1306_rect(&ssd, pos_y, pos_x,
                         SQUARE_SIZE, SQUARE_SIZE,
                         true, true);
        }

        ssd1306_send_data(&ssd);
        mostraDetalhes();
        sleep_ms(30);
    }
}

// Função de Callback
void gpio_callback(uint gpio, uint32_t events) {
    absolute_time_t now = get_absolute_time();
    int64_t diff = absolute_time_diff_us(last_interrupt_time, now);

    if (diff < 250000) return;
    last_interrupt_time = now;

    if (gpio == BTN_A) {
        pwm_on = !pwm_on;
        flagA  = !flagA;
        
    } else if (gpio == BTN_B) {
        borda = !borda;
        flagB = !flagB;
                
    } else if (gpio == BTN_JOY) {
        cursor_x = !cursor_x;
        flagJ = !flagJ;
    }
}

// Função do Joystick
void JOYSTICK(uint slice1, uint slice2) {
    const uint16_t CENTER = 2047;
    const uint16_t DEADZONE = 170;  // zona morta do Joystick

    // Lê o eixo Y (ADC0)
    adc_select_input(0);
    uint16_t y_value = adc_read();
    
    // Lê o eixo X (ADC1)
    adc_select_input(1);
    uint16_t x_value = adc_read();
    
    int16_t x_diff = (int16_t)x_value - CENTER;
    int16_t y_diff = (int16_t)y_value - CENTER;

    // Corrigindo o movimento no eixo X
    if (abs(x_diff) > DEADZONE) {
        pos_x += (x_diff > 0) ? SPEED : -SPEED;
        pos_x = (pos_x < 0) ? 0 : (pos_x > MAX_X) ? MAX_X : pos_x;
    }
    
    // Corrigindo o movimento no eixo Y
    if (abs(y_diff) > DEADZONE) {
        pos_y += (y_diff > 0) ? -SPEED : SPEED;  
        pos_y = (pos_y < 0) ? 0 : (pos_y > MAX_Y) ? MAX_Y : pos_y;
    }

    // Verificação do pwm em relação a deadzone
    uint16_t pwm_y = (abs(y_diff) <= DEADZONE) ? 0 : abs(y_diff) * 2;
    uint16_t pwm_x = (abs(x_diff) <= DEADZONE) ? 0 : abs(x_diff) * 2;

    if (pwm_on) {
        pwm_set_gpio_level(LED_B, pwm_y);
        pwm_set_gpio_level(LED_R, pwm_x);
    }
}

// Função print
void mostraDetalhes(){
	if (flagA){
	printf ("O botão A foi pressionado\n");
	flagA = !flagA;
	}

	if (flagB){
	printf("O botão B foi pressionado\n");
	flagB = !flagB;
	}

	if (flagJ){
	printf("O botão do Joystick foi pressionado\n");
	flagJ = !flagJ;
	}
}