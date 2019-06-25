#include <Arduino.h>
#include <Wire.h>
#include <Catena.h>
#include <RTClib.h>

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
    static const unsigned getTimeConstant() { return 1000000; }
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

    ledCount = 0;
}

void loop() {
    gCatena.poll();

    gpio.setBlue(digitalRead(A0));

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

    if (pirPrintTimer.isready())
        {
        float v = (pir.read() + 1.0f) / 2.0f;
        DateTime now = rtc.now();

        gCatena.SafePrintf("%02d:%02d:%02d", now.hour(), now.minute(), now.second());
        Serial.print(" PIR: ");
        Serial.print(v * 100.0f);
        Serial.println("%");
        }
}
