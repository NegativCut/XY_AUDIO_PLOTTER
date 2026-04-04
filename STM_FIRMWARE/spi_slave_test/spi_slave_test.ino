// Blue Pill SPI1 Slave - loopback test
// PA4=NSS, PA5=SCK, PA6=MISO, PA7=MOSI
// Responds to 8-byte transfer with known pattern 0x00..0x77
// LED (PC13) blinks on each completed transfer

uint8_t txBuf[8] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

void setup() {
  // Enable clocks: SPI1 + GPIOA
  RCC->APB2ENR |= RCC_APB2ENR_SPI1EN | RCC_APB2ENR_IOPAEN;

  // PA4 (NSS)  - input floating:       CNF=01, MODE=00
  // PA5 (SCK)  - input floating:       CNF=01, MODE=00
  // PA6 (MISO) - AF push-pull output:  CNF=10, MODE=11 (50 MHz)
  // PA7 (MOSI) - input floating:       CNF=01, MODE=00
  GPIOA->CRL &= ~(0xFFFF0000);
  GPIOA->CRL |=  (0x4B400000);

  // SPI1: slave mode, 8-bit, MSB first, CPOL=0 CPHA=0, hardware NSS
  SPI1->CR1 = 0;
  SPI1->CR2 = 0;
  SPI1->CR1 |= SPI_CR1_SPE;

  pinMode(PC13, OUTPUT);
  digitalWrite(PC13, HIGH); // LED off (active low)

  // Pre-load first byte
  SPI1->DR = txBuf[0];
}

void loop() {
  // Wait for full 8-byte transfer driven by Pi
  for (int i = 0; i < 8; i++) {
    while (!(SPI1->SR & SPI_SR_RXNE));
    (void)SPI1->DR;
    if (i + 1 < 8) SPI1->DR = txBuf[i + 1];
  }

  // Reload first byte for next transfer
  SPI1->DR = txBuf[0];

  // Blink LED to confirm transfer
  digitalWrite(PC13, LOW);
  delay(30);
  digitalWrite(PC13, HIGH);
}
