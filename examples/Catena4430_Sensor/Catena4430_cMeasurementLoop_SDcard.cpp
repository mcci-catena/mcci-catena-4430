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

#include <Catena_Fram.h>

#include <Arduino_LoRaWAN_lmic.h>

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
    True if an update was done and the system should be rebooted.

Notes:
    

*/

#define FUNCTION "cMeasurementLoop::handleSdFirmwareUpdate"

bool
cMeasurementLoop::handleSdFirmwareUpdate(
    void
    )
    {
    if (this->m_pSPI2 == nullptr)
        gLog.printf(gLog.kBug, "SPI2 not registered, can't program flash\n");

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
            gLog.printf(gLog.kTrace, "%s: applied update from %s: %s\n", FUNCTION, s, result ? "true": "false");
        return result;
        }

    return false;
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

    gLog.printf(gLog.kInfo, "Attempting to load firmware from %s\n", sUpdate);

    // power management: typically the SPI2 is powered down by a sleep,
    // and it's not powered back up when we wake up. The SPI flash is on
    // SPI2, and so we must power it up. We also have to deal with the
    // corner case where the SPI flash didn't probe correctly.
    if (this->m_pSPI2)
        {
        // SPI was found, bring it up.
        this->m_pSPI2->begin();
        // and bring up the flash.
        gFlash.begin(this->m_pSPI2, Catena::PIN_SPI2_FLASH_SS);
        }
    else
        {
        // something went wrong at boot time, we can't do anything
        // with firwmare update...
        gLog.printf(gLog.kError, "SPI2 pointer is null, give up\n");
        return false;
        }

    // try to open the file
    context.firmwareFile = gSD.open(sUpdate, FILE_READ);

    if (! context.firmwareFile)
        {
        // hmm. it exists but we could not open it.
        gLog.printf(gLog.kError, "%s: exists but can't open: %s\n", FUNCTION, sUpdate);
        return false;
        }

    // the downloader requires a "request block" that tells it what to do.
    // since we loop in this function, we can allocate it as a local variable,
    // and keep it in the context object. Save some typing by defining an
    // alias:
    auto & request = context.request;

    // The downloader is abstract; it doesn't know where data is coming from.
    // It calls various callbacks to get data and orchestrate the collection
    // of image data. So we (the client) must provide some callbacks. This
    // is done in a consistent way:
    //
    //  request.{callback}.init({pfn}, {pUserData});
    //
    // This initializes the specified callback with the specified function
    // and user data.
    //
    // In the code below, we use C++ lambdas to save useless static functions,
    // and also (nicely) to write code that is "inside" cMeasurementLoop and
    // can access protected and private items.
    //
    // The four callbacks are: QueryAvailableData, PromptForData, ReadBytes,
    // and Completion.
    //

    // initialize the "query available data" callback. We always say
    // kTransferChunkBytes are available, becuase we're reading from
    // a file. But this means we must fill buffer to max size in read
    // when we hit end of file.
    request.QueryAvailableData.init(
        [](void *pUserData) -> int
            {
            return cDownload::kTransferChunkBytes;
            },
        nullptr
        );
    
    // initalize the "prompt for more data" callback; we don't need one
    // when reading from a file.
    request.PromptForData.init(nullptr, nullptr);

    // initialize the read-byte callback
    request.ReadBytes.init(
        // this is called each time the downloader wants more data
        [](void *pUserData, std::uint8_t *pBuffer, size_t nBuffer) -> size_t
            {
            context_t * const pCtx = (context_t *)pUserData;

            gLog.printf(gLog.kInfo, ".");
            gCatena.poll();

            auto n = pCtx->firmwareFile.readBytes(pBuffer, nBuffer);
            if (n < nBuffer)
                {
                // at end of file we have spare bytes that are not
                // used. Initialize to 0xFF because that's nice for
                // SPI flash.
                memset(pBuffer + n, 0xFF, nBuffer - n);
                gLog.printf(gLog.kInfo, "\n");
                }

            return nBuffer;
            },
        (void *)&context
        );

    // initialize the "operation complete" callback. Just set
    // a flag to get us out of the wait loop.
    request.Completion.init(
        [](void *pUserData, cDownload::Status_t status) -> void
            {
            context_t * const pCtx = (context_t *)pUserData;

            pCtx->status = status;
            pCtx->fWorking = false;
            },
        (void *)&context
        );

    // set the request code in the request.
    request.rq = rq;

    // launch the request.
    if (! gDownload.evStart(request))
        {
        // It didn't launch. No callbacks will happen. Clean up.
        context.firmwareFile.close();
        // remove the file, so we don't get stuck in a loop.
        gSD.remove(sUpdate);
        // no need to reboot.
        return false;
        }

    // it launched: wait for transfer to complete
    while (context.fWorking)
        // give other clients a chance to look in.
        // and allow the download to be coded asynchronously
        // if necessary.
        gCatena.poll();

    // download operation is complete.
    // close and remove the file
    context.firmwareFile.close();
    gSD.remove(sUpdate);

    // if it failed, display the error code.
    if (context.status != cDownload::Status_t::kSuccessful)
        {
        gLog.printf(gLog.kError, "download failed, status %u\n", std::uint32_t(context.status));
        // no need to reboot.
        return false;
        }
    // if it succeeeded, say so, and tell caller to reboot.
    // don't reboot here, because the outer app may need to shut things down
    // in an orderly way.
    else
        {
        gLog.printf(gLog.kInfo, "download succeded.\n");
        return true;
        }
    }

void
cMeasurementLoop::handleSdTTNv3Migrate(
    void
    )
    {
    static const char * const sMigrate = "MIGRATE.V3";
    bool fMigrate;
    bool fResult = this->checkSdCard();

	if (fResult)
        {
        fMigrate = this->handleSdTTNv3MigrateCardUp(sMigrate);
        }

    if (fMigrate)
        {
        bool fFramUpdate = this->updateFramAppEui();

        if (fFramUpdate)
            {
            this->reJoinNetwork();
            gSD.remove(sMigrate);
            gLog.printf(gLog.kInfo, "cFramStorage::kAppEUI: update: success\n");
			}
        else
            gLog.printf(gLog.kError, "cFramStorage::kAppEUI: not updated\n");
        }
    }

bool
cMeasurementLoop::handleSdTTNv3MigrateCardUp(
    const char *sMigrate
    )
    {
    if (! gSD.exists(sMigrate))
        {
        if (gLog.isEnabled(gLog.kTrace))
            gLog.printf(gLog.kAlways, "%s: not found: %s\n", FUNCTION, sMigrate);

        return false;
        }
    return true;
    }

bool
cMeasurementLoop::updateFramAppEui(
    void
    )
    {
    auto const pFram = gCatena.getFram();
    uint8_t AppEUI[] = { 1, 0, 0, 0, 0, 0, 0, 0 };

    if (pFram == nullptr)
        return false;

    pFram->saveField(cFramStorage::kAppEUI, AppEUI);
    return true;
    }

void
cMeasurementLoop::reJoinNetwork(
    void
    )
    {
    auto const pFram = gCatena.getFram();
    pFram->saveField(cFramStorage::kDevAddr, 0);

    LMIC_unjoin();
    LMIC_startJoining();
    }

#undef FUNCTION
