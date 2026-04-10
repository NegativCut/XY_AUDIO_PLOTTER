// Blue Pill SPI1 Slave — CE0 link test
// PA4=NSS, PA5=SCK, PA6=MISO, PA7=MOSI
//
// Responds to each 16-byte transfer with a fixed known sequence:
//   0x00 0x11 0x22 0x33 0x44 0x55 0x66 0x77
//   0x88 0x99 0xAA 0xBB 0xCC 0xDD 0xEE 0xFF
//
// Pi reads 16 bytes and validates — if they match, SPI link is good.
// LED (PC13) blinks once per completed transfer.
// SPI TX driven by DMA1 Channel 3 for reliable byte timing.
//
// Version: 0.1.0

#define LED_PIN PC13
#define BUF_LEN 16

static const uint8_t txBuf[BUF_LEN] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
};

static uint8_t rxBuf[BUF_LEN];

static void dma_setup() {
    // Enable DMA1 clock
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    // --- TX: DMA1 Channel 3 (SPI1_TX) ---
    DMA1_Channel3->CCR = 0;
    DMA1_Channel3->CPAR = (uint32_t)&SPI1->DR;
    DMA1_Channel3->CMAR = (uint32_t)txBuf;
    DMA1_Channel3->CNDTR = BUF_LEN;
    DMA1_Channel3->CCR =
        DMA_CCR_MINC |       // memory increment
        DMA_CCR_DIR  |       // memory -> peripheral
        DMA_CCR_EN;          // enable

    // --- RX: DMA1 Channel 2 (SPI1_RX) ---
    DMA1_Channel2->CCR = 0;
    DMA1_Channel2->CPAR = (uint32_t)&SPI1->DR;
    DMA1_Channel2->CMAR = (uint32_t)rxBuf;
    DMA1_Channel2->CNDTR = BUF_LEN;
    DMA1_Channel2->CCR =
        DMA_CCR_MINC |       // memory increment
                             // direction = peripheral -> memory (default)
        DMA_CCR_EN;          // enable
}

static void dma_reload() {
    // Disable channels to reset
    DMA1_Channel3->CCR &= ~DMA_CCR_EN;
    DMA1_Channel2->CCR &= ~DMA_CCR_EN;

    DMA1_Channel3->CNDTR = BUF_LEN;
    DMA1_Channel2->CNDTR = BUF_LEN;

    DMA1_Channel3->CCR |= DMA_CCR_EN;
    DMA1_Channel2->CCR |= DMA_CCR_EN;
}

void setup() {
    // Enable clocks: SPI1 + GPIOA
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN | RCC_APB2ENR_IOPAEN;

    // PA4 (NSS) input floating, PA5 (SCK) input floating,
    // PA6 (MISO) AF push-pull 50 MHz, PA7 (MOSI) input floating
    GPIOA->CRL &= ~(0xFFFF0000U);
    GPIOA->CRL |=  (0x4B440000U);

    // SPI1: slave, 8-bit, MSB first, CPOL=0, CPHA=0, hardware NSS
    SPI1->CR1 = 0;
    SPI1->CR2 = SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN;  // enable DMA requests
    SPI1->CR1 = SPI_CR1_SPE;

    dma_setup();

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);  // active low, start off
}

void loop() {
    // Wait for RX DMA to complete (all 16 bytes received)
    while (DMA1_Channel2->CNDTR > 0);

    // Reload DMA for next transfer
    dma_reload();

    digitalWrite(LED_PIN, LOW);
    delay(30);
    digitalWrite(LED_PIN, HIGH);
}
