#include "Particle.h"

#include "PublishQueueSpiFlashRK.h"

SYSTEM_THREAD(ENABLED);

SerialLogHandler logHandler(LOG_LEVEL_INFO, { // Logging level for non-application messages
	{ "app.pubq", LOG_LEVEL_TRACE }
});

// Pick a chip, port, and CS line
// SpiFlashISSI spiFlash(SPI, A2);
// SpiFlashWinbond spiFlash(SPI, A4);
SpiFlashMacronix spiFlash(SPI, A4);
// SpiFlashWinbond spiFlash(SPI1, D5);

const std::chrono::milliseconds publishPeriod = 1min;
unsigned long lastPublish = 0;
int counter = 0;

void publishCounter();


void setup() {
	// For testing purposes, wait 10 seconds before continuing to allow serial to connect
	// before doing PublishQueue setup so the debug log messages can be read.
	// waitFor(Serial.isConnected, 10000); delay(1000);
    
    spiFlash.begin();

	PublishQueueSpiFlashRK::instance()
        .withSpiFlash(&spiFlash, 0, 100 * 4096)
        .setup();

}

void loop() {
    PublishQueueSpiFlashRK::instance().loop();

    if (lastPublish == 0 || millis() - lastPublish >= publishPeriod.count()) {
        lastPublish = millis();
        publishCounter();
    }

}

void publishCounter() {
	Log.info("publishing counter=%d", counter);

	char buf[32];
	snprintf(buf, sizeof(buf), "%d", counter++);

	PublishQueueSpiFlashRK::instance().publish("testEvent", buf, WITH_ACK);
}
