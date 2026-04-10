// Blue Pill SPI1 Slave — dummy XY Lissajous data via DMA
// PA4=NSS, PA5=SCK, PA6=MISO, PA7=MOSI
//
// Sends 1024 XY pairs (4096 bytes) per transaction.
// Format: [X_hi, X_lo, Y_hi, Y_lo] × 1024  (big-endian uint16, 12-bit, 0–4095)
// Phase advances each transfer so the Pi sees animated Lissajous motion.
//
// Pi side: spidev CE0 (GPIO 8), 16 MHz, mode 0, read 4096 bytes per frame.
// Note: Linux spidev default bufsiz is 4096 bytes — no module param needed.
//
// Version: 0.0.2

#include <math.h>

#define LED_PIN   PC13
#define SAMPLES   1024
#define BUF_SIZE  (SAMPLES * 4)   // 4 bytes per XY pair
#define SIN_LEN   (SAMPLES * 2)   // two full periods for a=1,b=2 Lissajous

static uint8_t  txBuf[BUF_SIZE];
static uint8_t  rxBuf[BUF_SIZE];   // drain master's MOSI to prevent OVR
static uint16_t sinTab[SIN_LEN];   // precomputed sin LUT, 12-bit scaled (0–4095)
static int      phaseIdx = 0;      // phase offset in LUT steps

// --- Buffer generation ---------------------------------------------------
// Lissajous a=1, b=2: x=sinTab[(i + phaseIdx) % SIN_LEN],
//                     y=sinTab[(2*i) % SIN_LEN]

static void build_lut() {
    for (int i = 0; i < SIN_LEN; i++) {
        float angle = 2.0f * (float)M_PI * i / SIN_LEN;
        sinTab[i] = (uint16_t)((sinf(angle) + 1.0f) * 2047.5f);
    }
}

static void update_buffer() {
    for (int i = 0; i < SAMPLES; i++) {
        uint16_t xi = sinTab[(i + phaseIdx) % SIN_LEN];
        uint16_t yi = sinTab[(2 * i)        % SIN_LEN];
        txBuf[i * 4 + 0] = (uint8_t)(xi >> 8);
        txBuf[i * 4 + 1] = (uint8_t)(xi & 0xFF);
        txBuf[i * 4 + 2] = (uint8_t)(yi >> 8);
        txBuf[i * 4 + 3] = (uint8_t)(yi & 0xFF);
    }
    phaseIdx = (phaseIdx + 10) % SIN_LEN;
}

// --- DMA -----------------------------------------------------------------
// DMA1 Channel 3 = SPI1_TX

static void arm_dma() {
    // TX
    DMA1_Channel3->CCR  &= ~DMA_CCR_EN;
    DMA1->IFCR           = DMA_IFCR_CGIF3;
    DMA1_Channel3->CMAR  = (uint32_t)txBuf;
    DMA1_Channel3->CNDTR = BUF_SIZE;
    DMA1_Channel3->CCR  |= DMA_CCR_EN;

    // RX
    DMA1_Channel2->CCR  &= ~DMA_CCR_EN;
    DMA1->IFCR           = DMA_IFCR_CGIF2;
    DMA1_Channel2->CMAR  = (uint32_t)rxBuf;
    DMA1_Channel2->CNDTR = BUF_SIZE;
    DMA1_Channel2->CCR  |= DMA_CCR_EN;
}

static void setup_dma() {
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    // TX: DMA1 Channel 3 — memory → peripheral
    DMA1_Channel3->CCR  = 0;
    DMA1_Channel3->CPAR = (uint32_t)&SPI1->DR;
    DMA1_Channel3->CCR  = DMA_CCR_DIR | DMA_CCR_MINC;

    // RX: DMA1 Channel 2 — peripheral → memory
    DMA1_Channel2->CCR  = 0;
    DMA1_Channel2->CPAR = (uint32_t)&SPI1->DR;
    DMA1_Channel2->CCR  = DMA_CCR_MINC;
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
    SPI1->CR2 = SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN;  // TX+RX DMA requests
    SPI1->CR1 = SPI_CR1_SPE;
}

// -------------------------------------------------------------------------

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);   // active-low, start off

    build_lut();
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
