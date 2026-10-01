/**
 * @file    Usart9.cpp
 * @brief   Implementación de Usart9 — ver Usart9.h
 *
 * Secuencia de begin() (idéntica a la de los drivers validados MDB_UART y
 * RS485 de VENDO_SLAVE, fase 0):
 *   1. Reloj del puerto GPIO y de la USART.
 *   2. TX y RX en función alternativa, velocidad muy alta, push-pull.
 *   3. UE=0 → CR1 = M | TE | RE → CR2 = 0 → CR3 = 0 → BRR → UE=1.
 *
 * El reloj del bus se calcula como HAL_RCC_GetPCLKxFreq():
 *   PCLK = SystemCoreClock >> APBPrescTable[PPREx]
 * así la librería no depende de la HAL.
 */

#include "Usart9.h"

namespace {

enum class ApbBus : uint8_t { None, Apb1, Apb2 };

ApbBus busOf(const USART_TypeDef* u) {
    if (u == USART2) return ApbBus::Apb1;
    if (u == USART1) return ApbBus::Apb2;
#if defined(USART6)
    if (u == USART6) return ApbBus::Apb2;
#endif
    return ApbBus::None;
}

void enableUsartClock(const USART_TypeDef* u) {
    if (u == USART2) { RCC->APB1ENR |= RCC_APB1ENR_USART2EN; (void)RCC->APB1ENR; }
    if (u == USART1) { RCC->APB2ENR |= RCC_APB2ENR_USART1EN; (void)RCC->APB2ENR; }
#if defined(USART6)
    if (u == USART6) { RCC->APB2ENR |= RCC_APB2ENR_USART6EN; (void)RCC->APB2ENR; }
#endif
}

void disableUsartClock(const USART_TypeDef* u) {
    if (u == USART2) RCC->APB1ENR &= ~RCC_APB1ENR_USART2EN;
    if (u == USART1) RCC->APB2ENR &= ~RCC_APB2ENR_USART1EN;
#if defined(USART6)
    if (u == USART6) RCC->APB2ENR &= ~RCC_APB2ENR_USART6EN;
#endif
}

/// Índice del puerto (GPIOA=0, GPIOB=1, …, GPIOH=7) o -1 si no es válido.
/// Los puertos están separados 0x400 en el bus AHB1 y el bit de reloj en
/// RCC->AHB1ENR coincide con ese índice.
int portIndex(const GPIO_TypeDef* port) {
    const uint32_t addr = reinterpret_cast<uint32_t>(port);
    if (addr < GPIOA_BASE) return -1;
    const uint32_t off = addr - GPIOA_BASE;
    if (off % 0x400u != 0u) return -1;
    const uint32_t idx = off / 0x400u;
    return (idx <= 7u) ? static_cast<int>(idx) : -1;
}

uint32_t apbClockHz(ApbBus bus) {
    const uint32_t cfgr = RCC->CFGR;
    const uint32_t ppre = (bus == ApbBus::Apb1)
        ? ((cfgr & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos)
        : ((cfgr & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos);
    return SystemCoreClock >> APBPrescTable[ppre];
}

/// Pin en función alternativa, velocidad muy alta, push-pull.
void configureAfPin(GPIO_TypeDef* port, uint8_t pin, uint8_t af, bool pullUp) {
    const uint32_t p2 = 2u * pin;
    const uint32_t p4 = 4u * (pin & 7u);

    port->MODER   = (port->MODER   & ~(3UL << p2)) | (2UL << p2);   // AF
    port->OTYPER &= ~(1UL << pin);                                   // push-pull
    port->OSPEEDR |= (3UL << p2);                                    // muy alta
    port->PUPDR   = (port->PUPDR   & ~(3UL << p2)) | ((pullUp ? 1UL : 0UL) << p2);
    port->AFR[pin >> 3] = (port->AFR[pin >> 3] & ~(0xFUL << p4))
                        | (static_cast<uint32_t>(af) << p4);
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

Usart9Status Usart9::begin(const Usart9Config& cfg) {
    _usart = nullptr;

    const ApbBus bus = busOf(cfg.instance);
    if (bus == ApbBus::None)                           return Usart9Status::InvalidInstance;
    const int pidx = portIndex(cfg.port);
    if (pidx < 0)                                      return Usart9Status::InvalidPort;
    if (cfg.txPin > 15 || cfg.rxPin > 15 || cfg.af > 15 || cfg.txPin == cfg.rxPin)
                                                       return Usart9Status::InvalidPin;
    if (cfg.baud == 0)                                 return Usart9Status::InvalidBaud;

    // ── 1. Relojes ────────────────────────────────────────────────────────
    RCC->AHB1ENR |= (1UL << pidx);
    (void)RCC->AHB1ENR;                   // lectura de sincronización tras habilitar reloj
    enableUsartClock(cfg.instance);

    // ── 2. Pines ─────────────────────────────────────────────────────────
    configureAfPin(cfg.port, cfg.txPin, cfg.af, false);
    configureAfPin(cfg.port, cfg.rxPin, cfg.af, cfg.rxPullUp);

    // ── 3. USART: 9 bits, sin paridad, 1 stop, x16 ───────────────────────
    USART_TypeDef* u = cfg.instance;
    u->CR1 &= ~USART_CR1_UE;
    u->CR1  = 0;
    u->CR1 |= USART_CR1_M;                // 9 bits de datos (F4: M, no M0)
    u->CR1 |= USART_CR1_TE;
    u->CR1 |= USART_CR1_RE;
    u->CR2  = 0;                          // 1 stop bit
    u->CR3  = 0;                          // sin DMA ni control de flujo

    const uint32_t pclk = apbClockHz(bus);
    const uint32_t brr  = (pclk + cfg.baud / 2u) / cfg.baud;   // redondeo al más cercano
    if (brr < 16u || brr > 0xFFFFu) {
        disableUsartClock(u);
        return Usart9Status::InvalidBaud;
    }
    u->BRR  = brr;
    u->CR1 |= USART_CR1_UE;

    _usart  = u;
    _pclkHz = pclk;
    _brr    = brr;
    _baud   = cfg.baud;
    return Usart9Status::Ok;
}

void Usart9::end() {
    if (!_usart) return;
    _usart->CR1 = 0;
    disableUsartClock(_usart);
    _usart = nullptr;
}

void Usart9::writeWord(uint16_t word) {
    while (!(_usart->SR & USART_SR_TXE)) { }
    _usart->DR = (word & WORD_MASK);
}

void Usart9::waitTxComplete() {
    while (!(_usart->SR & USART_SR_TC)) { }
}

// ── Recepción por polling ────────────────────────────────────────────────────

bool Usart9::rxAvailable() const {
    return _usart && (_usart->SR & (USART_SR_RXNE | USART_SR_ORE));
}

uint16_t Usart9::readWord() {
    if (!_usart) return 0;
    const uint32_t sr = _usart->SR;                  // 1) SR primero…
    if (!(sr & (USART_SR_RXNE | USART_SR_ORE))) return 0;
    const uint16_t dr = static_cast<uint16_t>(_usart->DR & WORD_MASK);   // 2) …luego DR: limpia flags

    uint16_t w = dr;
    if (sr & USART_SR_FE)  w |= FLAG_FE;
    if (sr & USART_SR_NE)  w |= FLAG_NE;
    if (sr & USART_SR_ORE) w |= FLAG_ORE;
    return w;
}

uint8_t Usart9::flushRx() {
    if (!_usart) return 0;
    uint8_t n = 0;
    // Con RXNE u ORE, la secuencia SR→DR deja el receptor limpio.
    // FE/NE sin RXNE no ocurren en F4 (se activan junto con RXNE).
    while (_usart->SR & (USART_SR_RXNE | USART_SR_ORE)) {
        (void)_usart->DR;
        if (n < 255) n++;
    }
    return n;
}
