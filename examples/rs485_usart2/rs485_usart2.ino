/**
 * Ejemplo: USART2 a 19200 baud, 9 bits, RS-485 half-duplex (Vendo SVE02).
 * PA2 TX, PA3 RX, PA1 DE+/RE. El control de DE queda fuera de la librería:
 * lo hace la aplicación (o una capa RS-485 propia).
 */
#include <Arduino.h>
#include <Usart9.h>

static Usart9 bus;
static const uint8_t DE_PIN = PA1;

static const Usart9Config BUS_CFG = { USART2, GPIOA, 2, 3, 7, 19200, true };

static void sendFrame(const uint16_t* words, uint8_t len) {
    digitalWrite(DE_PIN, HIGH);
    for (uint8_t i = 0; i < len; i++) bus.writeWord(words[i]);
    bus.waitTxComplete();          // stop bit del último carácter fuera
    digitalWrite(DE_PIN, LOW);     // bus libre para la respuesta
}

void setup() {
    pinMode(DE_PIN, OUTPUT);
    digitalWrite(DE_PIN, LOW);
    bus.begin(BUS_CFG);
}

void loop() {
    // Leer versión: 20* 03 07 00 2A
    static const uint16_t READ_VERSION[] = { 0x120, 0x003, 0x007, 0x000, 0x02A };
    sendFrame(READ_VERSION, 5);
    delay(1000);
}
