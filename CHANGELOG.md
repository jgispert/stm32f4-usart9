# Changelog

## 0.1.0 — pendiente de validación (fase 0.5 de VENDO_SLAVE_RS485)

- `Usart9Config`: instancia (USART1/2/6), puerto, pines, AF, baudrate y pull-up de RX.
- `begin()`: reloj GPIO + USART, pines en AF, 9 bits, sin paridad, 1 stop, x16. BRR redondeado al más cercano. El bus APB (APB1/APB2) se elige automáticamente.
- `writeWord()` (espera TXE) y `waitTxComplete()` (espera TC).
- `pclkHz()`, `brr()`, `baud()` para diagnóstico.
- Validación de parámetros con `Usart9Status`.
- Sin dependencias de Arduino ni HAL: `PCLK = SystemCoreClock >> APBPrescTable[PPREx]`.
- Ejemplos: MDB (USART1, 9600) y RS-485 Vendo (USART2, 19200).
