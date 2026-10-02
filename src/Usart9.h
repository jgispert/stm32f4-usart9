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
 * @version 0.3.0  — + modo por interrupciones (opcional): RX a un buffer
 *                    circular con marca de tiempo y TX asíncrona con aviso
 *                    al terminar (TC), para buses half-duplex.
 *          0.2.0  — + recepción por polling (rxAvailable/readWord/flushRx).
 *
 * ── Modo por interrupciones (v0.3.0) ────────────────────────────────────────
 *   static Usart9::RxEvent ring[128];                     // potencia de 2
 *   usart.begin(cfg);
 *   usart.enableIrq(USART2_IRQn, prioridad, ring, 128, miReloj);
 *   USART9_BIND_IRQ(USART2_IRQHandler, usart)             // en UN .cpp
 *
 *   Recepción: la interrupción guarda cada palabra con la hora de miReloj()
 *   (p. ej. DWT->CYCCNT). El programa la saca con rxPop(). Si el buffer se
 *   llena, la siguiente palabra guardada lleva FLAG_ORE (se perdió algo).
 *   Transmisión: txStart(words, n, onDone, ctx) envía sin bloquear; onDone()
 *   se llama DESDE LA INTERRUPCIÓN cuando sale el último stop bit (TC).
 *   rxMute(true) descarta lo recibido (útil mientras se transmite en
 *   half-duplex con RE ligado a DE).
 *
 *   Con STM32duino, compilar con -DHAL_UART_MODULE_ONLY: el core deja de
 *   definir sus USARTx_IRQHandler (HardwareSerial) y no hay conflicto.
 */

#ifndef STM32F4_USART9_H
#define STM32F4_USART9_H

#include <stdint.h>
#include "stm32f4xx.h"

#define USART9_VERSION "0.3.0"

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

    // ── Modo por interrupciones (v0.3.0) ────────────────────────────────
    struct RxEvent { uint16_t word; uint32_t t; };
    using TimeFn = uint32_t (*)();
    using DoneFn = void (*)(void* ctx);

    /// Activa la interrupción de recepción. `ring` (tamaño potencia de 2,
    /// máx. 32768) lo aporta el llamante y debe vivir mientras se use.
    /// `now` da la marca de tiempo de cada palabra (llamado en la ISR).
    void enableIrq(IRQn_Type irq, uint32_t priority, RxEvent* ring, uint16_t size, TimeFn now);

    /// Palabras pendientes en el buffer / saca la más antigua.
    uint16_t rxCount() const { return static_cast<uint16_t>(_head - _tail); }
    bool     rxPop(RxEvent& e);
    /// Vacía el buffer (lo recibido hasta ahora se descarta).
    void     rxDiscard() { _tail = _head; }
    /// Mientras esté activo, lo recibido se lee y se descarta en la ISR.
    void     rxMute(bool mute) { _mute = mute; }
    /// Palabras perdidas por buffer lleno desde begin().
    uint32_t rxOverflows() const { return _overflows; }

    /// Envía `n` palabras sin bloquear. `words` debe seguir válido hasta
    /// el aviso. `onDone(ctx)` se llama desde la ISR tras el último stop bit.
    /// Devuelve false si ya hay una transmisión en curso o n == 0.
    bool txStart(const uint16_t* words, uint16_t n, DoneFn onDone, void* ctx);
    bool txBusy() const { return _txBusy; }
    /// Cancela la transmisión en curso (p. ej. si TC no llega): sin aviso.
    void txAbort();

    /// Rutina de interrupción: llamarla desde USARTx_IRQHandler (ver USART9_BIND_IRQ).
    void irqHandler();

    static bool    hasError(uint16_t w) { return (w & FLAG_MASK) != 0; }
    static uint8_t dataOf(uint16_t w)   { return static_cast<uint8_t>(w & DATA_MASK); }
    static bool    bit9Of(uint16_t w)   { return (w & BIT9) != 0; }

private:
    USART_TypeDef* _usart  = nullptr;
    uint32_t       _pclkHz = 0;
    uint32_t       _brr    = 0;
    uint32_t       _baud   = 0;

    // Modo por interrupciones
    RxEvent*          _ring  = nullptr;
    uint16_t          _mask  = 0;
    TimeFn            _now   = nullptr;
    volatile uint16_t _head  = 0;          // escribe la ISR
    volatile uint16_t _tail  = 0;          // escribe el programa
    volatile bool     _mute  = false;
    volatile bool     _lost  = false;      // buffer lleno: marcar ORE en la siguiente
    volatile uint32_t _overflows = 0;
    const uint16_t*   _txBuf = nullptr;
    volatile uint16_t _txLen = 0;
    volatile uint16_t _txIdx = 0;
    volatile bool     _txBusy = false;
    DoneFn            _txDone = nullptr;
    void*             _txCtx  = nullptr;
};

/// Define el vector de interrupción `handler` (p. ej. USART2_IRQHandler)
/// para el objeto `obj`. Usar en un único .cpp del programa.
#define USART9_BIND_IRQ(handler, obj) \
    extern "C" void handler(void) { (obj).irqHandler(); }

#endif // STM32F4_USART9_H
