/*

Module: Catena4430_cMeasurementLoop_SDcard.cpp

Function:
    Class for running the SD Card

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   November 2019

*/

#include "Catena4430_cMeasurementLoop.h"

#include "Catena4430_Sensor.h"

#include <Catena_Download.h>

#include <SD.h>
#include <mcciadk_baselib.h>

using namespace McciCatena4430;
using namespace McciCatena;

/****************************************************************************\
|
|   globals
|
\****************************************************************************/

SDClass gSD;

/****************************************************************************\
|
|   Some utilities
|
\****************************************************************************/

// turn on power to the SD card
void cMeasurementLoop::sdPowerUp(bool fOn)
    {
    gpio.setVsdcard(fOn);
    }

void cMeasurementLoop::sdPrep()
    {
    digitalWrite(cMeasurementLoop::kSdCardCSpin, 1);
    pinMode(cMeasurementLoop::kSdCardCSpin, OUTPUT);
    if (! this->m_fSpi2Active)
        {
        this->m_pSPI2->begin();
        this->m_fSpi2Active = true;
        }

    digitalWrite(cMeasurementLoop::kSdCardCSpin, 1);
    this->sdPowerUp(true);
    delay(100);
    }

void cMeasurementLoop::sdFinish()
    {
    // gSD.end() calls card.forceIdle() which will
    // (try to) put the card in the idle state.
    if (! gSD.end())
        {
        gCatena.SafePrintf("gSD.end() timed out\n");
        }

    // turn off CS to avoid locking Vsdcard on.
    this->m_pSPI2->end();
    this->m_fSpi2Active = false;
    pinMode(Catena::PIN_SPI2_MOSI, OUTPUT);
    pinMode(Catena::PIN_SPI2_MISO, OUTPUT);
    pinMode(Catena::PIN_SPI2_SCK, OUTPUT);
    digitalWrite(Catena::PIN_SPI2_MOSI, 0);
    digitalWrite(Catena::PIN_SPI2_MISO, 0);
    digitalWrite(Catena::PIN_SPI2_SCK, 0);
    digitalWrite(cMeasurementLoop::kSdCardCSpin, 0);
    delay(1);
    this->sdPowerUp(false);
    }

/*

Name:   McciCatena4430::cMeasurementLoop::fillTxBuffer()

Function:
    Prepare a messages in a TxBuffer with data from current measurements.

Definition:
    void McciCatena4430::cMeasurementLoop::fillTxBuffer(
            cMeasurementLoop::TxBuffer_t& b
            );

Description:
    A format 0x22 message is prepared from the data in the cMeasurementLoop
    object.

*/

bool
cMeasurementLoop::initSdCard(
    )
    {
    bool fResult = this->checkSdCard();

    sdFinish();
    return fResult;
    }

bool
cMeasurementLoop::checkSdCard()
    {
    sdPrep();
    return gSD.begin(gSPI2, SPI_HALF_SPEED, kSdCardCSpin);
    }

static const char kHeader[] =
    "Time,DevEUI,Raw,Vbat,Vsystem,Vbus,BootCount,T,RH,P,Light,"
    "P[0].delta,P[0].total,P[1].delta,P[1].total,"
    "Act[7],Act[6],Act[5],Act[4],Act[3],Act[2],Act[1],Act[0]"
    "\n";

bool
cMeasurementLoop::writeSdCard(
    cMeasurementLoop::TxBuffer_t &b,
    cMeasurementLoop::Measurement const & mData
    )
    {
    cDate d;
    bool fResult;
    File dataFile;

    if (! mData.DateTime.isValid())
        {
        gCatena.SafePrintf("measurement time not valid\n");
        return false;
        }

    fResult = this->checkSdCard();
    if (! fResult)
        gCatena.SafePrintf("checkSdCard() failed\n");

    if (fResult)
        {
        // make a directory
        fResult = gSD.mkdir("Data");
        if (! fResult)
            gCatena.SafePrintf("mkdir failed\n");
        }

    if (fResult)
        {
        char fName[32];
        bool fNew;
        auto d = mData.DateTime;
        McciAdkLib_Snprintf(fName, sizeof(fName), 0, "Data/%04u%02u%02u.dat", d.year(), d.month(), d.day());

        fNew = !gSD.exists(fName);
        if (fNew)
            {
            //gCatena.SafePrintf("%s not found, will create & write header\n", fName);
            }

        File dataFile = gSD.open(fName, FILE_WRITE);
        if (dataFile)
            {
            if (fNew)
                {
                //gCatena.SafePrintf("write header\n");
                for (auto i : kHeader)
                    {
                    if (i == '\n')
                        {
                        dataFile.println();
                        }
                    else if (i == '\0')
                        {
                        break;
                        }
                    else
                        {
                        dataFile.print(i);
                        }
                    }
                }

            char buf[32];
            McciAdkLib_Snprintf(
                buf, sizeof(buf), 0,
                "%04u-%02u-%02uT%02u:%02u:%02uZ,",
                d.year(), d.month(), d.day(),
                d.hour(), d.minute(), d.second()
                );
            //gCatena.SafePrintf("write time\n");
            dataFile.print(buf);

            //gCatena.SafePrintf("write DevEUI");
            do  {
                CatenaBase::EUI64_buffer_t devEUI;

            	auto const pFram = gCatena.getFram();

                // use devEUI.
                if (pFram != nullptr &&
                    pFram->getField(cFramStorage::StandardKeys::kDevEUI, devEUI))
                    {
                    dataFile.print('"');

                    /* write the devEUI */
                    for (auto i = 0; i < sizeof(devEUI.b); ++i)
                        {
                        // the devEUI is stored in little-endian order.
                        McciAdkLib_Snprintf(
                            buf, sizeof(buf), 0,
                            "%02x", devEUI.b[sizeof(devEUI.b) - i - 1]
                            );
                        dataFile.print(buf);
                        }

                    dataFile.print('"');
                    }
                } while (0);

            dataFile.print(',');

            //gCatena.SafePrintf("write raw hex\n");
            dataFile.print('"');
            for (unsigned i = 0; i < b.getn(); ++i)
                {
                McciAdkLib_Snprintf(
                    buf, sizeof(buf), 0,
                    "%02x",
                    b.getbase()[i]
                    );
                dataFile.print(buf);
                }

            dataFile.print("\",");

            //gCatena.SafePrintf("write Vbat\n");
            if ((mData.flags & Flags::Vbat) != Flags(0))
               dataFile.print(mData.Vbat);

            dataFile.print(',');

            if ((mData.flags & Flags::Vcc) != Flags(0))
                dataFile.print(mData.Vsystem);

            dataFile.print(',');

            if ((mData.flags & Flags::Vbus) != Flags(0))
                dataFile.print(mData.Vbus);

            dataFile.print(',');

            if ((mData.flags & Flags::Boot) != Flags(0))
                dataFile.print(mData.BootCount);

            dataFile.print(',');

            if ((mData.flags & Flags::TPH) != Flags(0))
                {
                dataFile.print(mData.env.Temperature);
                dataFile.print(',');

                dataFile.print(mData.env.Humidity);
                dataFile.print(',');
                dataFile.print(mData.env.Pressure);
                dataFile.print(',');
                }
            else
                {
                dataFile.print(",,,");
                }

            if ((mData.flags & Flags::Light) != Flags(0))
                {
                dataFile.print(mData.light.White);
                }
            dataFile.print(',');

            for (auto const & feeder : mData.pellets)
                {
                if ((mData.flags & Flags::Pellets) != Flags(0))
                    dataFile.print(unsigned(feeder.Recent));
                dataFile.print(',');
                if ((mData.flags & Flags::Pellets) != Flags(0))
                    dataFile.print(feeder.Total);
                dataFile.print(',');
                }

            for (auto i = kMaxActivityEntries; i > 0; )
                {
                --i;
                if ((mData.flags & Flags::Activity) != Flags(0) &&
                    i < mData.nActivity)
                        dataFile.print(mData.activity[i].Avg);
                if (i > 0)
                    dataFile.print(',');
                }

            dataFile.println();
            dataFile.close();
            }
        else
            {
            gCatena.SafePrintf("can't open: %s\n", fName);
            }
        }

    sdFinish();
    return fResult;
    }

/*

Name:	cMeasurementLoop::handleSdFirmwareUpdate()

Index:  Name:   cMeasurementLoop::handleSdFirmwareUpdateCardUp()

Function:
    Check for firmware update request via SD card and handle it

Definition:
    bool cMeasurementLoop::handleSdFirmwareUpdate(
        void
        );

    bool cMeasurementLoop::handleSdFirmwareUpdateCardUp(
        void
        );

Description:
    Check for a suitable file on the SD card. If found, copy to
    flash. If successful, set the update flag, and rename the
    update file so we won't consider it again.  handleSdFirmwareUpdateCardUp()
    is simply the inner method, to be called as a wrapper once power is
    up on the card.

Returns:
    True if an SD card was seen and all seemed in order (update
    pending or not). False if some kind of I/O error stopped progress.

Notes:
    

*/

#define FUNCTION "cMeasurementLoop::handleSdFirmwareUpdate"

bool
cMeasurementLoop::handleSdFirmwareUpdate(
    void
    )
    {
    if (this->m_pSPI2 == nullptr)
        gLog.printf(gLog.kTrace, "SPI2 not registered, can't program flash\n");

    bool fResult = this->checkSdCard();
    if (fResult)
        {
        fResult = this->handleSdFirmwareUpdateCardUp();
        }
    this->sdFinish();
    return fResult;
    }

#undef FUNCTION

#define FUNCTION "cMeasurementLoop::handleSdFirmwareUpdateCardUp"

bool
cMeasurementLoop::handleSdFirmwareUpdateCardUp(
    void
    )
    {
    static const char * const sUpdate[] = { "update.bin", "fallback.bin" };

    for (auto s : sUpdate)
        {
        if (! gSD.exists(s))
            {
            if (gLog.isEnabled(gLog.kTrace))
                gLog.printf(gLog.kAlways, "%s: not found: %s\n", FUNCTION, s);
            continue;
            }

        auto result = this->updateFromSd(
                            s,
                            s[0] == 'u' ? cDownload::DownloadRq_t::GetUpdate
                                        : cDownload::DownloadRq_t::GetFallback
                            );
        if (gLog.isEnabled(gLog.kTrace))
            gLog.printf(gLog.kAlways, "%s: applied update from %s: %s\n", FUNCTION, s, result ? "true": "false");
        return result;
        }

    return true;
    }

bool
cMeasurementLoop::updateFromSd(
    const char *sUpdate,
    cDownload::DownloadRq_t rq
    )
    {
    // launch a programming cycle. We'll stall the measurement FSM here while
    // doing the operation, but poll the other FSMs.
    struct context_t
            {
            cMeasurementLoop *pThis;
            bool fWorking;
            File firmwareFile;
            cDownload::Status_t status;
            cDownload::Request_t request;
            };
    context_t context { this, true };

    if (gLog.isEnabled(gLog.kTrace))
        {
        gLog.printf(gLog.kAlways, "Attempting to load firmware from %s\n", sUpdate);
        }

    if (this->m_pSPI2)
        {
        this->m_pSPI2->begin();
        gFlash.begin(this->m_pSPI2, Catena::PIN_SPI2_FLASH_SS);
        }
    else
        {
        gLog.printf(gLog.kError, "SPI2 pointer is null, give up\n");
        return false;
        }

    // try to open the file
    context.firmwareFile = gSD.open(sUpdate, FILE_READ);

    if (! context.firmwareFile)
        {
        // hmm. it exists but we could not open it.
        if (gLog.isEnabled(gLog.kError))
            gLog.printf(gLog.kAlways, "%s: exists but can't open: %s\n", FUNCTION, sUpdate);
        return false;
        }

    auto & request = context.request;

    request.Completion.init(
        [](void *pUserData, cDownload::Status_t status) -> void
            {
            context_t * const pCtx = (context_t *)pUserData;

            pCtx->status = status;
            pCtx->fWorking = false;
            },
        (void *)&context
        );

    request.QueryAvailableData.init(
        [](void *pUserData) -> int
            {
            return cDownload::kTransferChunkBytes;
            },
        nullptr
        );
    
    request.PromptForData.init(nullptr, nullptr);

    request.ReadBytes.init(
        [](void *pUserData, std::uint8_t *pBuffer, size_t nBuffer) -> size_t
            {
            context_t * const pCtx = (context_t *)pUserData;

            gLog.printf(gLog.kInfo, ".");

            auto n = pCtx->firmwareFile.readBytes(pBuffer, nBuffer);
            if (n < nBuffer)
                {
                memset(pBuffer + n, 0xFF, nBuffer - n);
                gLog.printf(gLog.kInfo, "\n");
                }

            return n;
            },
        (void *)&context
        );

    request.rq = rq;

    if (! gDownload.evStart(request))
        {
        context.firmwareFile.close();
        gSD.remove(sUpdate);
        return false;
        }

    // wait for transfer to complete
    while (context.fWorking)
        gCatena.poll();

    if (context.status != cDownload::Status_t::kSuccessful)
        {
        gLog.printf(gLog.kError, "download failed, status %u\n", std::uint32_t(context.status));
        }

    // close and remove the file
    context.firmwareFile.close();
    gSD.remove(sUpdate);

    return true;
    }

#undef FUNCTION
