#include <Arduino.h>
#include <Wire.h>
#include <Catena.h>
#include <Catena_Mx25v8035f.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>

extern McciCatena::Catena gCatena;

/****************************************************************************\
|
|   PCA9570 I2C output buffer
|
\****************************************************************************/

class cPCA9570
    {
public:
    static constexpr std::uint8_t kI2cAddress = 0x24;

private:
    static constexpr std::uint8_t kActiveBits = 0xF;

public:
    cPCA9570(TwoWire *pWire) : m_i2caddr(kI2cAddress), m_wire(pWire), m_inversion(0) {};

    bool begin();
    void end();
    bool set(std::uint8_t value);
    bool modify(std::uint8_t mask, std::uint8_t bits);
    bool setPolarity(std::uint8_t mask)
        { this->m_inversion = (~mask) & kActiveBits; }
    std::uint8_t getPolarity() const
        { return this->m_inversion ^ kActiveBits; }
    std::uint8_t get() const
        { return this->m_value & kActiveBits; }
    bool read(std::uint8_t &value) const;

private:
    TwoWire *m_wire;
    std::uint8_t m_i2caddr;
    std::uint8_t m_value;
    std::uint8_t m_inversion;
    };

bool cPCA9570::begin()
    {
    this->m_wire->begin();

    return this->set(0);
    }

void cPCA9570::end()
    {
    (void) this->set(0);
    }

bool cPCA9570::modify(std::uint8_t mask, std::uint8_t bits)
    {
    return this->set((this->get() & ~mask) | (bits & mask));
    }

bool cPCA9570::set(std::uint8_t value)
    {
    int error;

    this->m_wire->beginTransmission(this->m_i2caddr);
    this->m_wire->write((value & kActiveBits) ^ this->m_inversion);
    error = this->m_wire->endTransmission();

    if (error != 0)
        return false;
    else
        {
        this->m_value = value & kActiveBits;
        return true;
        }
    }

bool cPCA9570::read(std::uint8_t &value) const
    {
    unsigned nRead;

    nRead = this->m_wire->requestFrom(this->m_i2caddr, uint8_t(1));

    if (nRead == 0)
        return false;
    else
        {
        value = (this->m_wire->read() ^ this->m_inversion) & kActiveBits;
        return true;
        }
    }

/****************************************************************************\
|
|   The 4430 GPIO cluster -- I2C GPIOs plus other LEDs
|
\****************************************************************************/

class c4430Gpios
    {
private:
    static const unsigned kRedLed = D12;
    static const unsigned kDisplayLed = D13;

public:
    static const std::uint8_t kDisplayMask  = (1 << 0);
    static const std::uint8_t kRedMask      = (1 << 1);
    static const std::uint8_t kBlueMask     = (1 << 2);
    static const std::uint8_t kGreenMask    = (1 << 3);

private:
    static const std::uint8_t kVout1Mask   = (1 << 0);
    static const std::uint8_t kVout2Mask   = (1 << 1);

public:
    c4430Gpios(cPCA9570 *pPCA9570) : m_gpio(pPCA9570) {};

    bool begin();
    void end();

    bool setBlue(bool fOn)  { return this->m_gpio->modify(kBlueMask, fOn ? 0xFF : 0); }
    bool setGreen(bool fOn) { return this->m_gpio->modify(kGreenMask, fOn ? 0xFF : 0); }
    bool setRed(bool fOn)   { digitalWrite(kRedLed, fOn); return true; }
    bool setDisplay(bool fOn) { digitalWrite(kDisplayLed, fOn); return true; }

    bool setVout1(bool fOn) { return this->m_gpio->modify(kVout1Mask, fOn ? 0xFF : 0); }
    bool getVout1() const   { return this->m_gpio->get() & kVout1Mask; }
    bool setVsdcard(bool fOn) { return this->m_gpio->modify(kVout2Mask, fOn ? 0xFF : 0); }
    bool getVsdcard() const { return this->m_gpio->get() & kVout2Mask; }

    bool setLeds(std::uint8_t mask, std::uint8_t v);
    std::uint8_t getLeds();

private:
    cPCA9570 *m_gpio;
    };

bool c4430Gpios::begin()
    {
    bool fResult;

    this->m_gpio->setPolarity(this->m_gpio->getPolarity() & ~(kBlueMask | kGreenMask));

    fResult = this->m_gpio->begin();

    digitalWrite(kRedLed, 0);
    pinMode(kRedLed, OUTPUT);

    digitalWrite(kDisplayLed, 0);
    pinMode(kDisplayLed, OUTPUT);

    return fResult;
    }

void c4430Gpios::end()
    {
    this->m_gpio->end();
    }

bool c4430Gpios::setLeds(std::uint8_t mask, std::uint8_t v)
    {
    bool fResult = true;

    fResult &= this->m_gpio->modify(mask & (kBlueMask | kGreenMask), v);
    if (mask & kRedMask)
        fResult &= this->setRed(v & kRedMask);
    if (mask & kDisplayMask)
        fResult &= this->setDisplay(v & kDisplayMask);

    return fResult;
    }

std::uint8_t c4430Gpios::getLeds()
    {
    std::uint8_t v;

    v = this->m_gpio->get() & (kBlueMask | kGreenMask);

    if (digitalRead(kRedLed))
        v |= kRedMask;
    if (digitalRead(kDisplayLed))
        v |= kDisplayMask;

    return v;
    }

/****************************************************************************\
|
|   A simple PIR library -- this uses cPollableObject because it's easier
|
\****************************************************************************/

class cPIRdigital : public McciCatena::cPollableObject
    {
    // the filtering time-constant, in microseconds.
    static const unsigned getTimeConstant() { return 1000000; }
    // the digital pin used for the PIR.
    static const unsigned kPirData = A0;

public:
    // constructor
    cPIRdigital(int pin = kPirData) 
        : m_pin(pin)
        { }

    // neither copyable nor movable 
    cPIRdigital(const cPIRdigital&) = delete;
    cPIRdigital& operator=(const cPIRdigital&) = delete;
    cPIRdigital(const cPIRdigital&&) = delete;
    cPIRdigital& operator=(const cPIRdigital&&) = delete;

    // initialze
    bool begin();

    // stop operation
    void end();

    // poll function (updates data)
    virtual void poll() override;

    // get a reading
    float read();

private:
    unsigned m_pin;
    std::uint32_t m_tLast;
    float m_value;
    };

bool cPIRdigital::begin()
    {
    pinMode(this->m_pin, INPUT);
    this->m_value = 0.0;
    this->m_tLast = micros();

    // set up for polling.
    gCatena.registerObject(this);
    }

void cPIRdigital::poll() /* override */
    {
    auto const v = digitalRead(this->m_pin);
    std::uint32_t tNow = micros();

    float delta = v ? 1.0f : -1.0f;
    
    float m = float(tNow - this->m_tLast) / this->getTimeConstant(); 

    // classic FIR filter is g*new + (1-g)*old
    // and that's what this is, if g is the effective gain.
    // note that g is adjusted based on the variable sampling
    // rate so that the overall time constnant is this->getTimeConstant().
    this->m_value = this->m_value + m * (delta  - this->m_value);

    if (this->m_value > 1.0f) 
        this->m_value = 1.0f;
    else if (this->m_value < -1.0f)
        this->m_value = -1.0f;

    this->m_tLast = tNow;
    }

float cPIRdigital::read()
    {
    return this->m_value;
    }

/****************************************************************************\
|
|   A simple timer -- this uses cPollableObject because it's easier
|
\****************************************************************************/

class cTimer : public McciCatena::cPollableObject
    {
public:
    // constructor
    cTimer() {}

    // neither copyable nor movable 
    cTimer(const cTimer&) = delete;
    cTimer& operator=(const cTimer&) = delete;
    cTimer(const cTimer&&) = delete;
    cTimer& operator=(const cTimer&&) = delete;

    // initialze to fire every nMillis
    bool begin(std::uint32_t nMillis);

    // stop operation
    void end();

    // poll function (updates data)
    virtual void poll() override;

    bool isready();
    std::uint32_t readTicks();
    std::uint32_t peekTicks() const;

    void debugDisplay() const
        {
        Serial.print("time="); Serial.print(this->m_time);
        Serial.print(" interval="); Serial.print(this->m_interval);
        Serial.print(" events="); Serial.print(this->m_events);
        Serial.print(" overrun="); Serial.println(this->m_overrun); 
        }

private:
    std::uint32_t   m_time;
    std::uint32_t   m_interval;
    std::uint32_t   m_events;
    std::uint32_t   m_overrun;
    };

bool cTimer::begin(std::uint32_t nMillis)
    {
    this->m_interval = nMillis;
    this->m_time = millis();
    this->m_events = 0;

    // set up for polling.
    gCatena.registerObject(this);

    return true;
    }

void cTimer::poll() /* override */
    {
    auto const tNow = millis();

    if (tNow - this->m_time >= this->m_interval)
        {
        this->m_time += this->m_interval;
        ++this->m_events;

        /* if this->m_time is now in the future, we're done */
        if (std::int32_t(tNow - this->m_time) < std::int32_t(this->m_interval))
            return;

        // rarely, we need to do arithmetic. time and events are in sync.
        // arrange for m_time to be greater than tNow, and adjust m_events
        // accordingly. 
        std::uint32_t const tDiff = tNow - this->m_time;
        std::uint32_t const nTicks = tDiff / this->m_interval;
        this->m_events += nTicks;
        this->m_time += nTicks * this->m_interval;
        this->m_overrun += nTicks;
        }
    }

bool cTimer::isready()
    {
    return this->readTicks() != 0;
    }

std::uint32_t cTimer::readTicks()
    {
    auto const result = this->m_events;
    this->m_events = 0;
    return result; 
    }

std::uint32_t cTimer::peekTicks() const
    {
    return this->m_events;
    }

/****************************************************************************\
|
|   Variables.
|
\****************************************************************************/

using namespace McciCatena;

cPCA9570 i2cgpio    { &Wire };
c4430Gpios gpio     { &i2cgpio };
Catena gCatena;

cTimer pirPrintTimer;
cTimer ledTimer;
cPIRdigital pir;

RTC_PCF8523 rtc;

SPIClass gSPI2(
    Catena::PIN_SPI2_MOSI,
    Catena::PIN_SPI2_MISO,
    Catena::PIN_SPI2_SCK
    );

//   The flash
Catena_Mx25v8035f gFlash;
bool gfFlash;

unsigned ledCount;

/****************************************************************************\
|
|   Code
|
\****************************************************************************/

void setup() {
    Wire.begin();
    Serial.begin(115200);
    while (! Serial)
        delay(1);
    Serial.println("\ncatena4430-testgpio");

    gCatena.begin();
    setup_flash();

    if (! gpio.begin())
        Serial.println("GPIO failed to initialize");

    pirPrintTimer.begin(2000);
    ledTimer.begin(200);

    if (! pir.begin())
        Serial.println("PIR failed to initialize");

    if (! rtc.begin())
        Serial.println("RTC failed to intiailize");

    DateTime now = rtc.now();
    gCatena.SafePrintf("RTC is %s. Date: %d-%02d-%02d %02d:%02d:%02d\n",
        rtc.initialized() ? "running" : "not initialized",
        now.year(), now.month(), now.day(),
        now.hour(), now.minute(), now.second()
        );

    // test the card
    CardInfo(D5);

    ledCount = 0;
}

void setup_flash(void)
    {
    if (gFlash.begin(&gSPI2, Catena::PIN_SPI2_FLASH_SS))
        {
        gfFlash = true;
        gFlash.powerDown();
        gCatena.SafePrintf("FLASH found, put power down\n");
        }
    else
        {
        gfFlash = false;
        gFlash.end();
        gSPI2.end();
        gCatena.SafePrintf("No FLASH found: check hardware\n");
        }
    }

void loop() {
    gCatena.poll();

    // copy current PIR state to the blue LED.
    gpio.setBlue(digitalRead(A0));

    // if it's time to update the LEDs, advance to the next step.
    if (ledTimer.isready())
        {
        const std::uint8_t ledMask = (gpio.kDisplayMask | gpio.kRedMask | gpio.kGreenMask);
        std::uint8_t ledValue;

        unsigned iGpio = 3 - ledCount;

        ledCount = (ledCount + 1) % 3;

        if (ledCount == 0)
            ledValue = gpio.kDisplayMask;
        else if (ledCount == 1)
            ledValue = gpio.kRedMask;
        else
            ledValue = gpio.kGreenMask;
    
        gpio.setLeds(ledMask, ledValue);
        }

    // if it's tiem to print the PIR state, printit out.
    if (pirPrintTimer.isready())
        {
        // for some reason, pir.read() returns a value in [-1, 1],
        // so we first map it to 0 to 1.
        float v = (pir.read() + 1.0f) / 2.0f;
        // get the current time from the RTC
        DateTime now = rtc.now();

        // print it out.
        gCatena.SafePrintf("%02d:%02d:%02d", now.hour(), now.minute(), now.second());
        Serial.print(" PIR: ");
        Serial.print(v * 100.0f);
        Serial.println("%");
        }
}


/****************************************************************************\
|
|   the cardinfo script
|
\****************************************************************************/

Sd2Card card;
SdVolume volume;
SdFile root;

// turn on power to the SD card
void sdPowerUp(bool fOn)
    {
    gpio.setVsdcard(fOn);
    }

void CardInfo(int chipSelect)
    {
    Serial.print("\nInitializing SD card...");

    digitalWrite(chipSelect, 1);
    digitalWrite(D7, 1);  // disable SX1276, wh

    pinMode(chipSelect, OUTPUT);

    sdPowerUp(true);
    delay(300);

    // we'll use the initialization code from the utility libraries
    // since we're just testing if the card is working!
    if (!card.init(gSPI2, SPI_HALF_SPEED, chipSelect))
    {
        Serial.println("initialization failed. Things to check:");
        Serial.println("* is a card inserted?");
        Serial.println("* is your wiring correct?");
        Serial.println("* did you change the chipSelect pin to match your shield or module?");
        sdPowerUp(false);
        return;
    }
    else
    {
        Serial.println("Wiring is correct and a card is present.");
    }

    // print the type of card
    Serial.println();
    Serial.print("Card type:         ");
    switch (card.type())
    {
    case SD_CARD_TYPE_SD1:
        Serial.println("SD1");
        break;
    case SD_CARD_TYPE_SD2:
        Serial.println("SD2");
        break;
    case SD_CARD_TYPE_SDHC:
        Serial.println("SDHC");
        break;
    default:
        Serial.println("Unknown");
    }

    // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
    if (!volume.init(card))
    {
        Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
        sdPowerUp(false);
        return;
    }

    Serial.print("Clusters:          ");
    Serial.println(volume.clusterCount());
    Serial.print("Blocks x Cluster:  ");
    Serial.println(volume.blocksPerCluster());

    Serial.print("Total Blocks:      ");
    Serial.println(volume.blocksPerCluster() * volume.clusterCount());
    Serial.println();

    // print the type and size of the first FAT-type volume
    uint32_t volumesize;
    Serial.print("Volume type is:    FAT");
    Serial.println(volume.fatType(), DEC);

    volumesize = volume.blocksPerCluster(); // clusters are collections of blocks
    volumesize *= volume.clusterCount();    // we'll have a lot of clusters
    volumesize /= 2;                        // SD card blocks are always 512 bytes (2 blocks are 1KB)
    Serial.print("Volume size (Kb):  ");
    Serial.println(volumesize);
    Serial.print("Volume size (Mb):  ");
    volumesize /= 1024;
    Serial.println(volumesize);
    Serial.print("Volume size (Gb):  ");
    Serial.println((float)volumesize / 1024.0);

    Serial.println("\nFiles found on the card (name, date and size in bytes): ");
    root.openRoot(volume);

    // list all files in the card with date and size
    root.ls(LS_R | LS_DATE | LS_SIZE);

    // clean things up.
    sdPowerUp(false);
    }
