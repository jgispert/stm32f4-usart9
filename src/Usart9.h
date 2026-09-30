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
 *
 * Por qué no HardwareSerial: su begin() reescribe CR1 y pierde el bit M
 * (9 bits de datos).
 *
 * @version 0.1.0  — begin/end y transmisión. La recepción llega en v0.2.0.
 */

#ifndef STM32F4_USART9_H
#define STM32F4_USART9_H

#include <stdint.h>
#include "stm32f4xx.h"

#define USART9_VERSION "0.1.0"

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

private:
    USART_TypeDef* _usart  = nullptr;
    uint32_t       _pclkHz = 0;
    uint32_t       _brr    = 0;
    uint32_t       _baud   = 0;
};

#endif // STM32F4_USART9_H
