# stm32f4-usart9

USART de **9 bits de datos** a nivel de registro para **STM32F4** (probado en F411CEU6 Black Pill).
Es la pieza común de los firmwares **VENDO_SLAVE_RS485** (USART2) y **MDB** (USART1).

> Estado: **v0.0.0, sin código aún.** La primera versión funcional (v0.1.0) se crea en la Fase 0.5 de VENDO_SLAVE_RS485.

## Principios

- Sólo depende de CMSIS (`stm32f4xx.h`) y de `HAL_RCC_GetPCLKxFreq()`.
- No usa nada de Arduino: ni `Serial`, ni `pinMode`, ni `delay`.
- No sabe nada de RS-485 (pin DE), MDB (checksum) ni USB. Esas capas van por encima.
- Una instancia activa por firmware. Todo se configura con una struct: no hay pines fijos en el código.
- Por qué no `HardwareSerial`: `begin()` reescribe CR1 y pierde el bit `M` (9 bits).

## Configuraciones de referencia

| Firmware | USART | TX | RX | AF | Bus | Baud |
|---|---|---|---|---|---|---|
| VENDO RS-485 | USART2 | PA2 | PA3 | 7 | APB1 | 19200 |
| MDB | USART1 | PA9 | PA10 | 7 | APB2 | 9600 |
| (disponible) | USART6 | PC6* | PC7* | 8 | APB2 | — |

\* En la Black Pill, PA11/PA12 (la otra opción de USART6) están ocupados por el USB. PC6/PC7 no salen al conector del F411CEU6 (encapsulado UFQFPN48), así que USART6 no es utilizable en esta placa.

## API prevista (borrador; se cierra en v0.1.0)

```cpp
struct Usart9Config {
    USART_TypeDef* instance;   // USART1 | USART2 | USART6
    GPIO_TypeDef*  port;       // GPIOA ...
    uint8_t        txPin;      // 2
    uint8_t        rxPin;      // 3
    uint8_t        af;         // 7
    uint32_t       baud;       // 19200
    bool           rxPullUp;   // true recomendado con transceptor half-duplex
};

// Palabra de 16 bits: [7:0] DATA · [8] D8 · [12] FE · [13] NE · [14] ORE
class Usart9 {
public:
    bool     begin(const Usart9Config& cfg);   // APB1/APB2 automático
    void     end();
    uint32_t pclkHz() const;  uint32_t brr() const;

    void     writeWord(uint16_t w);            // espera TXE
    void     waitTxComplete();                 // espera TC

    // v0.2.0 — polling
    bool     rxAvailable() const;
    uint16_t readWord();
    void     flushRx();                        // SR → DR (limpieza F4)

    // v0.3.0 — interrupciones
    void     enableRxIrq(uint16_t* ring, uint16_t size);
    void     onTxComplete(void (*cb)(void));
};
```

## Versiones previstas

| Versión | Contenido | Fase VENDO |
|---|---|---|
| v0.1.0 | begin / TX | 0.5 |
| v0.2.0 | RX por polling + flags de error | 1 |
| v0.3.0 | RX por ISR + callback de TC | 7 |
| v1.0.0 | Estable tras validación prolongada | 9 |

## Uso desde un proyecto PlatformIO

```ini
lib_deps = https://github.com/<owner>/stm32f4-usart9.git#v0.1.0
```

Para desarrollar la librería en local, en el `platformio_local.ini` del proyecto:

```ini
[env:blackpill_f411ce]
lib_deps = symlink://../stm32f4-usart9
```
