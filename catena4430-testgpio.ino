#include <Arduino.h>
#include <Wire.h>
#include <Catena.h>

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

    nRead = this->m_wire->requestFrom(this->m_i2caddr, 1);

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
|   The 4430 LED indicator
|
\****************************************************************************/

class c4430Gpios
    {
private:
    static const unsigned kRedLed = D12;
    static const unsigned kDisplayLed = D13;
    static const std::uint8_t kDisplayMask  = (1 << 0);
    static const std::uint8_t kRedMask      = (1 << 1);
    static const std::uint8_t kGreenMask    = (1 << 2);
    static const std::uint8_t kBlueMask     = (1 << 3);

    static const std::uint8_t kVout1Mask   = (1 << 0);
    static const std::uint8_t kVout2Mask   = (1 << 1);

public:
    c4430Gpios(cPCA9570 *pPCA9570) : m_gpio(pPCA9570) {};

    bool begin();
    void end();

    bool setBlue(bool fOn)  { return this->m_gpio->modify(kBlueMask, fOn ? 0xFF : 0); }
    bool setGreen(bool fOn) { return this->m_gpio->modify(kGreenMask, fOn ? 0xFF : 0); }
    bool setRed(bool fOn)   { digitalWrite(kRedLed, !fOn); return true; }
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

    this->m_gpio->setPolarity(this->m_gpio->getPolarity() | (kBlueMask | kGreenMask));

    fResult = this->m_gpio->begin();

    digitalWrite(kRedLed, 1);
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

    if (! digitalRead(kRedLed))
        v |= kRedMask;
    if (digitalRead(kDisplayLed))
        v |= kDisplayMask;

    return v;
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
}

void loop() {
    gCatena.poll();

    for (unsigned count = 4; count > 0; --count)
        {
        unsigned iGpio = 4 - count;

        gpio.setLeds(0xFF, 1 << iGpio);
        delay(100);
        }
}
