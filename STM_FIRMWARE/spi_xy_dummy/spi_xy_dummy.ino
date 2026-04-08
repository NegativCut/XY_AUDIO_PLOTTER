// Blue Pill SPI1 Slave — dummy XY Lissajous data via DMA
// PA4=NSS, PA5=SCK, PA6=MISO, PA7=MOSI
//
// Sends 1024 XY pairs (4096 bytes) per transaction.
// Format: [X_hi, X_lo, Y_hi, Y_lo] × 1024  (big-endian uint16, 12-bit, 0–4095)
// Phase advances each transfer so the Pi sees animated Lissajous motion.
//
// Pi side: spidev CE0 (GPIO 8), 16 MHz, mode 0, read 4096 bytes per frame.
// Note: Linux spidev default bufsiz is 4096 bytes — no module param needed.

#include <math.h>

#define LED_PIN  PC13
#define SAMPLES  1024
#define BUF_SIZE (SAMPLES * 4)   // 4 bytes per XY pair

static uint8_t txBuf[BUF_SIZE];
static float   phase = 0.0f;

// --- Buffer generation ---------------------------------------------------
// Lissajous a=1, b=2: x=sin(θ+phase), y=sin(2θ), scaled to 12-bit (0–4095)

static void update_buffer() {
    for (int i = 0; i < SAMPLES; i++) {
        float angle = 2.0f * (float)M_PI * i / SAMPLES;
        uint16_t xi = (uint16_t)((sinf(angle + phase) + 1.0f) * 2047.5f);
        uint16_t yi = (uint16_t)((sinf(2.0f * angle)  + 1.0f) * 2047.5f);
        txBuf[i * 4 + 0] = (uint8_t)(xi >> 8);
        txBuf[i * 4 + 1] = (uint8_t)(xi & 0xFF);
        txBuf[i * 4 + 2] = (uint8_t)(yi >> 8);
        txBuf[i * 4 + 3] = (uint8_t)(yi & 0xFF);
    }
    phase += 0.05f;
    if (phase >= 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
}

// --- DMA -----------------------------------------------------------------
// DMA1 Channel 3 = SPI1_TX

static void arm_dma() {
    DMA1_Channel3->CCR  &= ~DMA_CCR_EN;                     // disable to reconfigure
    DMA1->IFCR           = DMA_IFCR_CGIF3;                  // clear all ch3 flags
    DMA1_Channel3->CMAR  = (uint32_t)txBuf;
    DMA1_Channel3->CNDTR = BUF_SIZE;
    DMA1_Channel3->CCR  |= DMA_CCR_EN;                      // enable
}

static void setup_dma() {
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    DMA1_Channel3->CCR = 0;
    DMA1_Channel3->CCR = DMA_CCR_DIR   |   // memory → peripheral
                         DMA_CCR_MINC;     // increment memory address
    // PSIZE=00 (8-bit), MSIZE=00 (8-bit), CIRC=0, no interrupt
    DMA1_Channel3->CPAR = (uint32_t)&SPI1->DR;
}

// --- SPI1 slave ----------------------------------------------------------

static void setup_spi_slave() {
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN | RCC_APB2ENR_IOPAEN;

    // PA4 (NSS) input floating, PA5 (SCK) input floating,
    // PA6 (MISO) AF push-pull 50 MHz, PA7 (MOSI) input floating
    // CRL bits [31:16]: same magic value as the loopback test
    GPIOA->CRL &= ~(0xFFFF0000U);
    GPIOA->CRL |=  (0x4B440000U);   // PA7=0100 PA6=1011 PA5=0100 PA4=0100

    // Slave, 8-bit, MSB first, CPOL=0, CPHA=0, hardware NSS
    SPI1->CR1 = 0;
    SPI1->CR2 = SPI_CR2_TXDMAEN;   // TX DMA request enable
    SPI1->CR1 = SPI_CR1_SPE;
}

// -------------------------------------------------------------------------

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);   // active-low, start off

    update_buffer();
    setup_spi_slave();
    setup_dma();
    arm_dma();
}

void loop() {
    // Poll for DMA transfer complete (all BUF_SIZE bytes shifted out)
    if (DMA1->ISR & DMA_ISR_TCIF3) {
        DMA1_Channel3->CCR &= ~DMA_CCR_EN;    // disable before reconfigure
        DMA1->IFCR = DMA_IFCR_CGIF3;

        // Update phase and refill buffer for next transfer
        update_buffer();
        arm_dma();

        // Brief LED blink to confirm each completed frame
        digitalWrite(LED_PIN, LOW);
        delay(5);
        digitalWrite(LED_PIN, HIGH);
    }
}
