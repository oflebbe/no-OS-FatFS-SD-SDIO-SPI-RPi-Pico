/* Instead of a statically linked hw_config.c,
   create configuration dynamically */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <vector>
//
#include "f_util.h"
#include "ff.h"
#include "pico/stdlib.h"
#include "rtc.h"
//
#include "hw_config.h"
//
#include "diskio.h" /* Declarations of disk functions */
//
#include "SDIO/rp2040_sdio.h"
#include "rp2040_sdio.pio.h"

static std::vector<spi_t *> spis;
static std::vector<sd_card_t *> sd_cards;

size_t sd_get_num() { return sd_cards.size(); }
sd_card_t *sd_get_by_num(size_t num) {
    if (num <= sd_get_num()) {
        return sd_cards[num];
    } else {
        return NULL;
    }
}

size_t spi_get_num() { return spis.size(); }
spi_t *spi_get_by_num(size_t num) {
    if (num <= sd_get_num()) {
        return spis[num];
    } else {
        return NULL;
    }
}
void add_spi(spi_t *spi) { spis.push_back(spi); }
void add_sd_card(sd_card_t *sd_card) { sd_cards.push_back(sd_card); }

// Need a unique interrupt handler for each instance of spi_t
void spi0_dma_isr() {
    assert(spis.size());
    spi_irq_handler(spis[0]);
}
// sd_cards[1].sdio_if's DMA ISR
void sdio0_dma_isr() {
    assert(0 < sd_cards.size());
    rp2040_sdio_tx_irq(sd_cards[1]);
}
void test(sd_card_t *pSD) {
    // See FatFs - Generic FAT Filesystem Module, "Application Interface",
    // http://elm-chan.org/fsw/ff/00index_e.html
    FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
    if (FR_OK != fr) panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    fr = f_chdrive(pSD->pcName);
    if (FR_OK != fr) panic("f_chdrive error: %s (%d)\n", FRESULT_str(fr), fr);

    FIL fil;
    const char *const filename = "filename.txt";
    fr = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
    if (FR_OK != fr && FR_EXIST != fr)
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    if (f_printf(&fil, "Hello, world!\n") < 0) {
        printf("f_printf failed\n");
    }
    fr = f_close(&fil);
    if (FR_OK != fr) {
        printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    f_unmount(pSD->pcName);
}

int main() {
    stdio_init_all();
    time_init();

    puts("Hello, world!");

    // Hardware Configuration of SPI "object"
    spi_t *p_spi = new spi_t;
    memset(p_spi, 0, sizeof(spi_t));
    if (!p_spi) panic("Out of memory");
    p_spi->hw_inst = spi1;  // SPI component
    p_spi->miso_gpio = 12;  // GPIO number (not pin number)
    p_spi->mosi_gpio = 15;
    p_spi->sck_gpio = 14;
    p_spi->baud_rate = 25 * 1000 * 1000;  // Actual frequency: 20833333.
    p_spi->dma_isr = spi0_dma_isr;
    p_spi->initialized = false;  // initialized flag
    add_spi(p_spi);

    // Hardware Configuration of the SD Card "object"
    sd_card_t *p_sd_card = new sd_card_t;
    if (!p_sd_card) panic("Out of memory");
    memset(p_sd_card, 0, sizeof(sd_card_t));
    p_sd_card->pcName = "0:";  // Name used to mount device
    p_sd_card->type = SD_IF_SPI,
    p_sd_card->spi_if.spi = p_spi;  // Pointer to the SPI driving this card
    p_sd_card->spi_if.ss_gpio = 9;  // The SPI slave select GPIO for this SD card
    p_sd_card->use_card_detect = true;
    p_sd_card->card_detect_gpio = 13;  // Card detect
    // What the GPIO read returns when a card is
    // present. Use -1 if there is no card detect.
    p_sd_card->card_detected_true = 1;
    add_sd_card(p_sd_card);

    /* Add another SD card */
    p_sd_card = new sd_card_t;
    if (!p_sd_card) panic("Out of memory");
    memset(p_sd_card, 0, sizeof(sd_card_t));
    p_sd_card->pcName = "1:";  // Name used to mount device
    p_sd_card->type = SD_IF_SDIO;
    p_sd_card->sdio_if.CLK_gpio = SDIO_CLK_GPIO;  // From sd_driver/SDIO/rp2040_sdio.pio
    p_sd_card->sdio_if.CMD_gpio = 18;
    p_sd_card->sdio_if.D0_gpio = 19;
    p_sd_card->sdio_if.D1_gpio = 20;
    p_sd_card->sdio_if.D2_gpio = 21;
    p_sd_card->sdio_if.D3_gpio = 22;
    p_sd_card->use_card_detect = true;
    p_sd_card->card_detect_gpio = 16;   // Card detect
    p_sd_card->card_detected_true = 1;  // What the GPIO read returns when a card is present.
    p_sd_card->sdio_if.SDIO_PIO = pio1;
    p_sd_card->sdio_if.DMA_IRQ_num = DMA_IRQ_1;
    p_sd_card->sdio_if.dma_isr = sdio0_dma_isr;
    add_sd_card(p_sd_card);

    for (size_t i = 0; i < sd_get_num(); ++i)
        test(sd_get_by_num(i));

    puts("Goodbye, world!");

    for (;;)
        ;
}
