# stm32f4-usart9

USART de **9 bits de datos** a nivel de registro para **STM32F4** (probado en F411CEU6 Black Pill).
Es la pieza común de los firmwares **VENDO_SLAVE_RS485** (USART2) y **MDB** (USART1).

> Estado: **v0.1.0**. Configuración y transmisión. En validación en la fase 0.5 de VENDO_SLAVE_RS485.

## Principios

- Sólo depende de CMSIS (`stm32f4xx.h`, `SystemCoreClock`, `APBPrescTable`). No usa la HAL.
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

## API (v0.1.0)

```cpp
#include <Usart9.h>

static Usart9 bus;
static const Usart9Config CFG = {
    USART2,   // instance: USART1 | USART2 | USART6
    GPIOA,    // port (TX y RX en el mismo puerto)
    2, 3,     // txPin, rxPin
    7,        // af
    19200,    // baud
    false     // rxPullUp
};

if (bus.begin(CFG) != Usart9Status::Ok) { /* parámetros no válidos */ }

bus.writeWord(Usart9::makeWord(0x20, true));   // 0x120: dato 0x20 con bit 9
bus.writeWord(0x003);                          // dato 0x03 sin bit 9
bus.waitTxComplete();                          // stop bit del último carácter fuera

bus.pclkHz();  bus.brr();  bus.baud();         // diagnóstico
```

| Función | Descripción |
|---|---|
| `begin(cfg)` | Reloj GPIO + USART, pines en AF, 9 bits, sin paridad, 1 stop, x16. Devuelve `Usart9Status` |
| `end()` | Deshabilita la USART y su reloj |
| `writeWord(w)` | Espera TXE y escribe los 9 bits bajos de `w` |
| `waitTxComplete()` | Espera TC: el último carácter, stop incluido, ha salido |
| `makeWord(data, bit9)` | Compone la palabra de 9 bits |
| `pclkHz()`, `brr()`, `baud()` | Valores efectivos para diagnóstico |

El reloj del bus se calcula igual que `HAL_RCC_GetPCLKxFreq()` (`SystemCoreClock >> APBPrescTable[PPREx]`), sin depender de la HAL.

### Previsto

- v0.2.0: `rxAvailable()`, `readWord()` con flags FE/NE/ORE en la palabra, `flushRx()`.
- v0.3.0: RX por interrupción con ring buffer y callback de TC.

Ejemplos en `examples/`: MDB (USART1, 9600) y RS-485 Vendo (USART2, 19200).

## Versiones previstas

| Versión | Contenido | Fase VENDO |
|---|---|---|
| v0.1.0 | begin / TX (**actual**) | 0.5 |
| v0.2.0 | RX por polling + flags de error | 1 |
| v0.3.0 | RX por ISR + callback de TC | 7 |
| v1.0.0 | Estable tras validación prolongada | 9 |

## Uso desde un proyecto PlatformIO

```ini
lib_deps = https://github.com/jgispert/stm32f4-usart9.git#v0.1.0
```

Para desarrollar la librería en local, en el `platformio_local.ini` del proyecto:

```ini
[env:blackpill_f411ce]
lib_deps = symlink://../stm32f4-usart9
```
