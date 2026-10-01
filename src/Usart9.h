/**
 * @file    Usart9.h
 * @brief   USART de 9 bits de datos a nivel de registro para STM32F4
 *          (USART1, USART2, USART6).
 *
 * Principios:
 *   - Sólo depende de CMSIS (stm32f4xx.h). No usa Arduino ni HAL.
 *   - No sabe nada de RS-485 (pin DE), MDB (checksum) ni USB.
 *   - Toda la configuración entra por Usart9Config: sin pines fijos.
 *
 * Formato de palabra (uint16_t):
 *   bits 7..0  DATA
 *   bit  8     noveno bit (mode / address)
 *   bit  12    FE  — error de trama   ┐
 *   bit  13    NE  — ruido            ├ sólo en palabras RECIBIDAS
 *   bit  14    ORE — overrun          ┘
 *   Una palabra con error se entrega igualmente: decidir qué hacer con
 *   ella es cosa de la capa superior.
 *
 * Por qué no HardwareSerial: su begin() reescribe CR1 y pierde el bit M
 * (9 bits de datos).
 *
 * @version 0.2.0  — + recepción por polling (rxAvailable/readWord/flushRx).
 */

#ifndef STM32F4_USART9_H
#define STM32F4_USART9_H

#include <stdint.h>
#include "stm32f4xx.h"

#define USART9_VERSION "0.2.0"

// ── Configuración ────────────────────────────────────────────────────────────
struct Usart9Config {
    USART_TypeDef* instance;   // USART1 | USART2 | USART6
    GPIO_TypeDef*  port;       // Puerto de TX y RX (ambos en el mismo puerto)
    uint8_t        txPin;      // 0..15
    uint8_t        rxPin;      // 0..15
    uint8_t        af;         // Función alternativa (AF7 USART1/2, AF8 USART6)
    uint32_t       baud;       // p. ej. 19200 (RS-485 Vendo) o 9600 (MDB)
    bool           rxPullUp;   // Pull-up interno en RX
};

enum class Usart9Status : uint8_t {
    Ok = 0,
    InvalidInstance,   // No es USART1, USART2 ni USART6
    InvalidPort,       // Puerto GPIO no reconocido
    InvalidPin,        // Pin > 15, AF > 15 o TX == RX
    InvalidBaud,       // baud == 0 o BRR fuera de rango
};

// ── Driver ───────────────────────────────────────────────────────────────────
class Usart9 {
public:
    static constexpr uint16_t DATA_MASK = 0x00FF;
    static constexpr uint16_t BIT9      = 0x0100;
    static constexpr uint16_t WORD_MASK = 0x01FF;
    static constexpr uint16_t FLAG_FE   = 0x1000;   // Framing error
    static constexpr uint16_t FLAG_NE   = 0x2000;   // Noise
    static constexpr uint16_t FLAG_ORE  = 0x4000;   // Overrun: se perdió al menos una palabra ANTES de ésta
    static constexpr uint16_t FLAG_MASK = 0x7000;

    /// Configura GPIO, reloj y USART en 9 bits, sin paridad, 1 stop,
    /// sobremuestreo x16, TX y RX habilitados. No usa interrupciones ni DMA.
    Usart9Status begin(const Usart9Config& cfg);

    /// Deshabilita la USART y su reloj. Los pines quedan como estaban.
    void end();

    bool     isReady() const { return _usart != nullptr; }
    uint32_t pclkHz()  const { return _pclkHz; }   // Reloj del bus APB usado
    uint32_t brr()     const { return _brr;    }   // Valor escrito en BRR
    uint32_t baud()    const { return _baud;   }   // Baudrate solicitado

    /// Palabra de 9 bits a partir de un byte y el noveno bit.
    static uint16_t makeWord(uint8_t data, bool bit9) {
        return static_cast<uint16_t>((bit9 ? BIT9 : 0u) | data);
    }

    /// Escribe una palabra de 9 bits. Bloquea hasta que DR está libre (TXE).
    void writeWord(uint16_t word);

    /// Bloquea hasta que el último carácter ha salido por completo,
    /// stop bit incluido (flag TC). Úsalo antes de liberar un bus half-duplex.
    void waitTxComplete();

    // ── Recepción por polling (v0.2.0) ──────────────────────────────────
    // Sin interrupciones: con 9 bits a 19200 baud llega una palabra cada
    // ~573 µs, así que basta con consultar más a menudo que eso.

    /// true si hay una palabra pendiente de leer (RXNE) o un overrun.
    bool rxAvailable() const;

    /// Lee la palabra pendiente con sus flags de error (bits 12–14).
    /// Secuencia F4: leer SR y después DR, lo que limpia RXNE/ORE/NE/FE.
    /// Llamar sólo si rxAvailable(); si no, devuelve 0.
    uint16_t readWord();

    /// Descarta todo lo pendiente y limpia los flags de error.
    /// Devuelve cuántas palabras se han descartado.
    uint8_t flushRx();

    static bool    hasError(uint16_t w) { return (w & FLAG_MASK) != 0; }
    static uint8_t dataOf(uint16_t w)   { return static_cast<uint8_t>(w & DATA_MASK); }
    static bool    bit9Of(uint16_t w)   { return (w & BIT9) != 0; }

private:
    USART_TypeDef* _usart  = nullptr;
    uint32_t       _pclkHz = 0;
    uint32_t       _brr    = 0;
    uint32_t       _baud   = 0;
};

#endif // STM32F4_USART9_H
