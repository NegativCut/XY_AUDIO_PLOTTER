// Blue Pill SPI1 Slave — dual ADC + DMA -> SPI DMA
// Audio input: PA0 = Left/X (ADC1 Ch0), PA1 = Right/Y (ADC2 Ch1)
// SPI:         PA4 = NSS, PA5 = SCK, PA6 = MISO, PA7 = MOSI
//
// Collects 512 simultaneous XY pairs via dual regular ADC mode,
// triggered by TIM3 at 96 kS/s. Sends 2048 bytes per frame via SPI1 slave DMA.
// Format: [X_hi, X_lo, Y_hi, Y_lo] x 512  big-endian uint16, 12-bit (0-4095)
//
// Pi: spidev CE0 (GPIO 8), 16 MHz, mode 0, read 2048 bytes per frame.
// Bias: 1.65 V midpoint = ADC ~2048 = screen centre. No offset subtraction needed.
//
// Version: 1.1.0

#define LED_PIN   PC13
#define SAMPLES   512
#define BUF_SIZE  (SAMPLES * 4)   // bytes: [X_hi, X_lo, Y_hi, Y_lo] per pair

// ADC1->DR in dual mode: bits [15:0] = ADC1 (X/Left), bits [31:16] = ADC2 (Y/Right)
static uint32_t adcBuf[SAMPLES];
static uint8_t  txBuf[BUF_SIZE];
static uint8_t  rxBuf[BUF_SIZE];

static bool adcReady = false;

// --- Pack ADC results into SPI TX buffer (big-endian uint16) ---
static void pack_tx() {
    for (int i = 0; i < SAMPLES; i++) {
        uint16_t xi = (uint16_t)(adcBuf[i]         & 0x0FFF);  // ADC1 = X = Left
        uint16_t yi = (uint16_t)((adcBuf[i] >> 16) & 0x0FFF);  // ADC2 = Y = Right
        txBuf[i * 4 + 0] = xi >> 8;
        txBuf[i * 4 + 1] = xi & 0xFF;
        txBuf[i * 4 + 2] = yi >> 8;
        txBuf[i * 4 + 3] = yi & 0xFF;
    }
}

// --- Arm SPI1 DMA (TX + RX) ---
static void arm_spi_dma() {
    DMA1_Channel3->CCR &= ~DMA_CCR_EN;
    DMA1->IFCR = DMA_IFCR_CGIF3;
    DMA1_Channel3->CMAR  = (uint32_t)txBuf;
    DMA1_Channel3->CNDTR = BUF_SIZE;
    DMA1_Channel3->CCR  |= DMA_CCR_EN;

    DMA1_Channel2->CCR &= ~DMA_CCR_EN;
    DMA1->IFCR = DMA_IFCR_CGIF2;
    DMA1_Channel2->CMAR  = (uint32_t)rxBuf;
    DMA1_Channel2->CNDTR = BUF_SIZE;
    DMA1_Channel2->CCR  |= DMA_CCR_EN;
}

// --- Arm ADC1 DMA (one-shot, SAMPLES x 32-bit transfers) ---
static void arm_adc_dma() {
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;
    DMA1->IFCR = DMA_IFCR_CGIF1;
    DMA1_Channel1->CNDTR = SAMPLES;
    DMA1_Channel1->CCR  |= DMA_CCR_EN;
}

// -------------------------------------------------------------------------

static void setup_gpio() {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // PA0, PA1: analog input (CNF=00, MODE=00)
    GPIOA->CRL &= ~(GPIO_CRL_CNF0 | GPIO_CRL_MODE0 |
                    GPIO_CRL_CNF1 | GPIO_CRL_MODE1);

    // PA4=NSS input floating, PA5=SCK input floating,
    // PA6=MISO AF push-pull 50 MHz,  PA7=MOSI input floating
    GPIOA->CRL &= ~0xFFFF0000U;
    GPIOA->CRL |=  0x4B440000U;
}

static void setup_timer() {
    // TIM3 Update -> TRGO at 96 kHz: 72 MHz / 750 = 96 000 Hz
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    TIM3->PSC = 0;
    TIM3->ARR = 749;
    TIM3->CR2 = TIM_CR2_MMS_1;   // MMS = 010: Update event -> TRGO
    TIM3->CR1 = TIM_CR1_CEN;
}

static void setup_dma() {
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    // ADC1 DMA: DMA1 Channel 1, 32-bit, peripheral -> memory (no EN yet)
    DMA1_Channel1->CCR  = 0;
    DMA1_Channel1->CPAR = (uint32_t)&ADC1->DR;
    DMA1_Channel1->CMAR = (uint32_t)adcBuf;
    DMA1_Channel1->CCR  = DMA_CCR_MINC | DMA_CCR_MSIZE_1 | DMA_CCR_PSIZE_1;

    // SPI1 TX: DMA1 Channel 3, memory -> peripheral
    DMA1_Channel3->CCR  = 0;
    DMA1_Channel3->CPAR = (uint32_t)&SPI1->DR;
    DMA1_Channel3->CCR  = DMA_CCR_DIR | DMA_CCR_MINC;

    // SPI1 RX: DMA1 Channel 2, peripheral -> memory
    DMA1_Channel2->CCR  = 0;
    DMA1_Channel2->CPAR = (uint32_t)&SPI1->DR;
    DMA1_Channel2->CCR  = DMA_CCR_MINC;
}

static void setup_adc() {
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN;

    // ADC clock: PCLK2 / 6 = 72 / 6 = 12 MHz  (max 14 MHz)
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_ADCPRE) | RCC_CFGR_ADCPRE_DIV6;

    // ADC2 (slave): Ch1 (PA1 = Right/Y), 28.5-cycle sample, software trigger
    ADC2->SQR3  = 1;
    ADC2->SMPR2 = (3U << ADC_SMPR2_SMP1_Pos);
    ADC2->CR2   = ADC_CR2_EXTTRIG | (7U << ADC_CR2_EXTSEL_Pos);  // EXTSEL=111 (slaved)
    ADC2->CR2  |= ADC_CR2_ADON;

    // ADC1 (master): Ch0 (PA0 = Left/X), 28.5-cycle sample, TIM3_TRGO, DMA, dual mode
    ADC1->SQR3  = 0;
    ADC1->SMPR2 = (3U << ADC_SMPR2_SMP0_Pos);
    ADC1->CR1   = (6U << ADC_CR1_DUALMOD_Pos);  // DUALMOD = 0110 = regular simultaneous
    ADC1->CR2   = ADC_CR2_DMA | ADC_CR2_EXTTRIG | (4U << ADC_CR2_EXTSEL_Pos);  // EXTSEL=100 = TIM3_TRGO
    ADC1->CR2  |= ADC_CR2_ADON;

    // Stabilisation: >=2 us after ADON before calibration
    for (volatile int i = 0; i < 200; i++);

    // Calibrate ADC1
    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while (ADC1->CR2 & ADC_CR2_RSTCAL);
    ADC1->CR2 |= ADC_CR2_CAL;
    while (ADC1->CR2 & ADC_CR2_CAL);

    // Calibrate ADC2
    ADC2->CR2 |= ADC_CR2_RSTCAL;
    while (ADC2->CR2 & ADC_CR2_RSTCAL);
    ADC2->CR2 |= ADC_CR2_CAL;
    while (ADC2->CR2 & ADC_CR2_CAL);
}

static void setup_spi() {
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    SPI1->CR1 = 0;
    SPI1->CR2 = SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN;
    SPI1->CR1 = SPI_CR1_SPE;
}

// -------------------------------------------------------------------------

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);   // active-low, off

    setup_gpio();
    setup_timer();
    setup_dma();
    setup_adc();
    setup_spi();

    // Collect first ADC batch synchronously, then arm SPI
    arm_adc_dma();
    while (!(DMA1->ISR & DMA_ISR_TCIF1));
    DMA1->IFCR = DMA_IFCR_CGIF1;

    pack_tx();
    arm_spi_dma();
    arm_adc_dma();   // start collecting next batch
}

void loop() {
    // Poll ADC DMA complete
    if (DMA1->ISR & DMA_ISR_TCIF1) {
        DMA1->IFCR = DMA_IFCR_CGIF1;
        adcReady = true;
    }

    // Poll SPI TX DMA complete — Pi has finished reading this frame
    if (DMA1->ISR & DMA_ISR_TCIF3) {
        DMA1_Channel3->CCR &= ~DMA_CCR_EN;
        DMA1->IFCR = DMA_IFCR_CGIF3;

        if (adcReady) {
            adcReady = false;
            pack_tx();       // copy latest ADC batch to SPI TX buffer
            arm_adc_dma();   // start collecting next batch
        }
        // If adcReady is false, ADC is still collecting — resend last frame
        arm_spi_dma();

        digitalWrite(LED_PIN, LOW);
        delay(5);
        digitalWrite(LED_PIN, HIGH);
    }
}
