/************************************************
 *       File:   TV_Remote_1572_main.c          *
 *    Clock:  32MHz (INTOSC 8MHz + PLL 4x)      *
 * PWM: 38kHz @ 33% Duty (Hardware 16-bit PWM)  *
 ************************************************/


// PIC12F1572 Configuration Bit Settings
// 'C' source line config statements

// CONFIG1
#pragma config FOSC = INTOSC    // (INTOSC oscillator; I/O function on CLKIN pin)
#pragma config WDTE = OFF       // Watchdog Timer Disabled
#pragma config PWRTE = ON       // Power-up Timer Enable
#pragma config MCLRE = OFF      // MCLR/VPP pin function is digital input
#pragma config CP = OFF         // Flash Program memory code protection is disabled
#pragma config BOREN = OFF      // Brown-out Reset disabled
#pragma config CLKOUTEN = OFF   // CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin

// CONFIG2
#pragma config WRT = OFF        // Flash Memory Self-Write Protection off
#pragma config PLLEN = ON       // 4x PLL enabled
#pragma config STVREN = ON      // Stack Overflow or Underflow will cause a Reset
#pragma config BORV = LO        // Brown-out Reset Voltage (Vbor), low trip point selected
#pragma config LPBOREN = OFF    // Low Power Brown-out Reset disabled 
#pragma config DEBUG = OFF      // In-Circuit Debugger disabled, ICSPCLK and ICSPDAT are general purpose I/O pins
#pragma config LVP = OFF        // High-voltage on MCLR/VPP must be used for programming

#define _XTAL_FREQ 32000000  // Para __delay_us()

#define LED_SENDING PORTAbits.RA1 // Pino 6 do PIC

#include <xc.h>


// Arrays RAW (codigos divididos por 25)
//TCL
const uint8_t tcl_Power[] = {160, 160, 20, 78, 20, 78, 20, 78,
    20, 78, 20, 38, 20, 38, 20, 78, 20, 38, 20, 78, 20, 38, 20, 78, 20, 38,
    20, 38, 20, 38, 20, 38, 20, 38, 20, 78, 20, 78, 20, 38, 20, 78, 20, 38,
    20, 78, 20, 38, 20, 78, 20};

// SONY
const uint8_t sony_Power[] = {96, 22, 48, 24, 26, 22, 48, 22,
    26, 22, 50, 22, 26, 22, 26, 22, 50, 22, 26, 22, 26, 22, 26, 22, 26};

// PHILIPS
const uint8_t philips_Power[] = {108, 36, 18, 36, 20, 16, 20,
    16, 20, 34, 38, 16, 20, 16, 20, 16, 20, 16, 20, 16, 20, 16, 20, 16, 20,
    18, 18, 18, 18, 18, 18, 20, 16, 38, 16, 20, 34, 20, 16, 20};


// ==================== ENVIO PWM HIGH====================

void high(unsigned int us) {
    PWM2CONbits.OE = 1; // ON
    while (us--) __delay_us(1); // Tempo em us ligado
    PWM2CONbits.OE = 0; // OFF
}

// ==================== SPACE PWM LOW ====================

void low(unsigned int us) {
    while (us--) __delay_us(1); // Tempo em us esperando
}


// ==================== FUNÇÕES DE ENVIO ====================
// === NEC ===

void sendNEC(uint32_t code) {

    // Header NEC
    PWM2CONbits.OE = 1; // ON
    __delay_us(9000); // Tempo em us ligado
    PWM2CONbits.OE = 0; // OFF
    __delay_us(4500); // Tempo em us desligado

    for (uint8_t i = 0; i < 32; i++) {

        PWM2CONbits.OE = 1; // ON
        __delay_us(560); // Tempo em us ligado
        PWM2CONbits.OE = 0; // OFF

        (code & 1) ? __delay_us(1690) : __delay_us(560); // Desligado
        code >>= 1;
    }

    // Pulso final:
    PWM2CONbits.OE = 1; // ON
    __delay_us(560); // Tempo em us ligado
    PWM2CONbits.OE = 0; // OFF
    PORTAbits.RA0 = 0; // RA0 desligado.
    __delay_us(1000); // Stop bit
}

// ==== Send Raw ===

void sendRaw(const uint8_t *signal, uint8_t len) {
    // Cheguei no valor 11 impiricamente usando o osciloscopio e o arduino oara ler os pulsos
    for (uint8_t i = 0; i < len; i++) {
        if (i % 2 == 0) high(signal[i] * 11); // HIGH (portadora ativa) 
        else low(signal[i] * 11); // LOW (portadora inativa)

    }
    PWM2CONbits.OE = 0; // Garante portadora desligada ao final
    PORTAbits.RA0 = 0; // RA0 desligado.
}

// ==================== INTERRUPÇÃO EXTERNA ====================

void __interrupt() ISR(void) {

    if (INTCONbits.INTF) {
        INTCONbits.INTF = 0; // Limpa flag
        INTCONbits.INTE = 0; // Desabilita INT temporariamente

        PWM2CONbits.EN = 1; // Liga o PWM 

        LED_SENDING = 1; // LED Sending = 1;

        __delay_ms(10); // Debounce

        // === Sequência de envio ===
        sendNEC(0xFD020707UL);
        __delay_ms(100); // Samsung
        sendNEC(0xF708FB04UL);
        __delay_ms(100); // LG
        sendNEC(0xE817C7EAUL);
        __delay_ms(100); // Philco
        sendNEC(0xFE01BD00UL);
        __delay_ms(100); // AOC
        sendNEC(0xED12BF40UL);
        __delay_ms(100); // SEMP/TOSHIBA

        sendRaw(tcl_Power, sizeof (tcl_Power));
        __delay_ms(100); // TCL

        sendRaw(philips_Power, sizeof (philips_Power));
        __delay_ms(84);
        sendRaw(philips_Power, sizeof (philips_Power));
        __delay_ms(100); // Philips 2x

        sendRaw(sony_Power, sizeof (sony_Power));
        __delay_ms(25);
        sendRaw(sony_Power, sizeof (sony_Power));
        __delay_ms(100); // Sony 2x

        LED_SENDING = 0; // LED Sending = 0;

        __delay_ms(1000); // Anti-repetição

        INTCONbits.INTE = 1; // Reabilita INT
        INTCONbits.INTF = 0;

        PWM2CONbits.EN = 0; // Desliga o PWM 
        PORTAbits.RA0 = 0; // RA0 desligado.
    }
}

// ==================== MAIN ====================

void main(void) {

    // Configuracao do oscilador para 32MHz:
    OSCCONbits.SCS = 0b00; // Internal oscillator
    OSCCONbits.IRCF = 0b1110; // 8 MHz HFINTOSC
    OSCCONbits.SPLLEN = 1; // PLL 4x enable

    //Bits de status em OSCSTAT
    while (!OSCSTATbits.HFIOFR); // Aguarda HFINTOSC ready
    while (!OSCSTATbits.HFIOFS); // Aguarda frequência estável

    // APFCON para pin select
    // PWM1 pode sair em RA1 (padrão) ou RA5 (se P1SEL=1)
    // PWM2 em RA0
    // APFCON: P1SEL=0 (PWM1 em RA1), P2SEL=0 (PWM2 em RA0)
    APFCON = 0b00000000;

    // Configura RA0 como saída digital (LED IR - PWM)
    TRISAbits.TRISA0 = 0; // Saida digital em RA0 - Pino 7 do PIC
    ANSELAbits.ANSA0 = 0; // Desabilita ADC no pino RA0
    WPUAbits.WPUA0 = 0; // Pull-up desabilidado no RA0
    PORTAbits.RA0 = 0; // RA0 desligado.

    // Configura RA1 como saída digital (LED SENDING)
    TRISAbits.TRISA1 = 0; // Saida digital em RA1 - Pino 6 do PIC
    ANSELAbits.ANSA1 = 0; // Desabilita ADC no pino RA1
    WPUAbits.WPUA1 = 0; // Pull-up desabilidado no RA1
    PORTAbits.RA1 = 0; // RA1 desligado.

    // Configura RA2 como entrada digital (BOTAO)
    TRISAbits.TRISA2 = 1; // Entrada digital em RA1 - Pino 5 do PIC
    ANSELAbits.ANSA2 = 0; // Desabilita ADC no pino RA1
    WPUAbits.WPUA2 = 0; // Pull-up desabilidado no RA1

    // Configuracao do PWM2:
    // === 3. Configurar PWM2 para 38kHz @ 33% duty ===
    PWM2CLKCON = 0b00000000; // Clock = HFINTOSC (32MHz)
    PWM2CON = 0b11000000; // Module+Output enabled, active-high, Standard mode
    PWM2PH = 0; // Phase = 0
    PWM2PR = 842; // Período: 32MHz/842 = 38.004 kHz
    PWM2DC = 280; // Duty: 280/842 = 33.3%
    PWM2LD = 1; // Carregar duty no buffer ativo

    // Interrupção externa em RA2/INT (borda de subida)
    OPTION_REGbits.INTEDG = 1; // 1 = rising edge
    INTCONbits.INTF = 0;
    INTCONbits.INTE = 1;
    INTCONbits.GIE = 1;

    __delay_ms(100); // Estabilização inicial

    while (1) {
        SLEEP(); // Ultra-baixo consumo (~20nA) [[datasheet]]
        // Acorda apenas com INT do botão
        asm("nop"); // recomendado pelo data sheet
    }
}