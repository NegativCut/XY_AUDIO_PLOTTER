// Blue Pill SPI1 Slave — CE0 link test
// PA4=NSS, PA5=SCK, PA6=MISO, PA7=MOSI
//
// Responds to each 16-byte transfer with a fixed known sequence:
//   0x00 0x11 0x22 0x33 0x44 0x55 0x66 0x77
//   0x88 0x99 0xAA 0xBB 0xCC 0xDD 0xEE 0xFF
//
// Pi reads 16 bytes and validates — if they match, SPI link is good.
// LED (PC13) blinks once per completed transfer.

#define LED_PIN PC13
#define BUF_LEN 16

static const uint8_t txBuf[BUF_LEN] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
};

void setup() {
    // Enable clocks: SPI1 + GPIOA
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN | RCC_APB2ENR_IOPAEN;

    // PA4 (NSS) input floating, PA5 (SCK) input floating,
    // PA6 (MISO) AF push-pull 50 MHz, PA7 (MOSI) input floating
    GPIOA->CRL &= ~(0xFFFF0000U);
    GPIOA->CRL |=  (0x4B440000U);

    // SPI1: slave, 8-bit, MSB first, CPOL=0, CPHA=0, hardware NSS
    SPI1->CR1 = 0;
    SPI1->CR2 = 0;
    SPI1->CR1 = SPI_CR1_SPE;

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);  // active low, start off

    // Pre-load first byte before master can assert CS
    SPI1->DR = txBuf[0];
}

void loop() {
    for (int i = 0; i < BUF_LEN; i++) {
        while (!(SPI1->SR & SPI_SR_RXNE));  // wait for byte clocked in
        (void)SPI1->DR;                      // discard received byte
        if (i + 1 < BUF_LEN) {
            SPI1->DR = txBuf[i + 1];         // load next byte
        }
    }

    // Reload for next transfer
    SPI1->DR = txBuf[0];

    digitalWrite(LED_PIN, LOW);
    delay(30);
    digitalWrite(LED_PIN, HIGH);
}
