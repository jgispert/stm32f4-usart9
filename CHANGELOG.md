# Changelog

## 0.3.0 — 2026-10-05

- Modo por interrupciones, **opcional** (el polling de 0.2.0 no cambia):
  - RX: `enableIrq()` con buffer circular de `RxEvent {word, t}` aportado por el llamante; `rxCount()`, `rxPop()`, `rxDiscard()`, `rxMute()`, `rxOverflows()`. Si el buffer se llena, la siguiente palabra guardada lleva `FLAG_ORE`.
  - TX: `txStart()` asíncrona por TXE con aviso `onDone` desde la ISR al llegar TC (para bajar DE en ≈ 1 µs); `txBusy()`, `txAbort()`.
  - `irqHandler()` y la macro `USART9_BIND_IRQ(handler, obj)`.
- Requiere `-DHAL_UART_MODULE_ONLY` con STM32duino (vector USARTx_IRQHandler).

Validada en VENDO_SLAVE_RS485 fase 7 (RS485-070…075): DE liberado en la ISR de TC a 53,3–53,4 µs del bit de stop (≈ 1 µs más que por polling, 0,1 µs de dispersión); 30 min a 20 ms = 84 089 ciclos sin pérdidas (`rxOverflows() = 0`).

## 0.2.0 — 2026-10-01

Validada en VENDO_SLAVE_RS485 fase 1 (RS485-010…013: 14/14 respuestas de la máquina recibidas sin errores, D8 correcto).

- Recepción por polling: `rxAvailable()`, `readWord()`, `flushRx()`.
- Las palabras recibidas llevan sus flags de error en los bits 12–14 (`FLAG_FE`, `FLAG_NE`, `FLAG_ORE`). Una palabra con error **se entrega igualmente**.
- Utilidades: `hasError()`, `dataOf()`, `bit9Of()`.

## 0.1.0 — 2026-10-01

Validada en VENDO_SLAVE_RS485 fase 0.5 (RS485-005, 006 y 007: BRR idéntico, regresión TX en máquina real y compilación de los ejemplos).

- `Usart9Config`: instancia (USART1/2/6), puerto, pines, AF, baudrate y pull-up de RX.
- `begin()`: reloj GPIO + USART, pines en AF, 9 bits, sin paridad, 1 stop, x16. BRR redondeado al más cercano. El bus APB (APB1/APB2) se elige automáticamente.
- `writeWord()` (espera TXE) y `waitTxComplete()` (espera TC).
- `pclkHz()`, `brr()`, `baud()` para diagnóstico.
- Validación de parámetros con `Usart9Status`.
- Sin dependencias de Arduino ni HAL: `PCLK = SystemCoreClock >> APBPrescTable[PPREx]`.
- Ejemplos: MDB (USART1, 9600) y RS-485 Vendo (USART2, 19200).
