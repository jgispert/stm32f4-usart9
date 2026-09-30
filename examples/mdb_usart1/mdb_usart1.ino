/**
 * Ejemplo: USART1 a 9600 baud, 9 bits (MDB) — PA9 TX, PA10 RX.
 * Sólo demuestra la configuración: envía un RESET MDB (0x10 con bit 9)
 * y su checksum (0x10 sin bit 9) cada segundo.
 */
#include <Arduino.h>
#include <Usart9.h>

static Usart9 mdb;

static const Usart9Config MDB_CFG = {
    USART1,     // instance  (APB2)
    GPIOA,      // port
    9,          // txPin     PA9
    10,         // rxPin     PA10
    7,          // af        AF7
    9600,       // baud
    true        // rxPullUp
};

void setup() {
    Serial.begin(115200);
    if (mdb.begin(MDB_CFG) != Usart9Status::Ok) {
        Serial.println("Usart9: configuración no válida");
        return;
    }
    Serial.print("USART1 listo  PCLK=");
    Serial.print(mdb.pclkHz());
    Serial.print(" Hz  BRR=");
    Serial.println(mdb.brr());
}

void loop() {
    if (!mdb.isReady()) return;
    mdb.writeWord(Usart9::makeWord(0x10, true));    // RESET, modo dirección
    mdb.writeWord(Usart9::makeWord(0x10, false));   // CHK
    mdb.waitTxComplete();
    delay(1000);
}
