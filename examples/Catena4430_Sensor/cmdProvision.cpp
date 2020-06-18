/*

Module: Catena4430_cmdProvision.cpp

Function:
    Process the "provision" command.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Dhinesh Kumar Pitchai, MCCI Corporation   June 2020

*/

#include "Catena4430_cmd.h"

#include "Catena4430.h"
#include <CatenaBase.h>
#include <cstring>
#include <strings.h>
#include <Catena_FlashParam.h>
#include <mcciadk_baselib.h>

using namespace McciCatena;

extern void start_provisioning(uint32_t delay_s);

/*

Name:   ::cmdProvision()

Function:
    Command dispatcher for "mode" command.

Definition:
    McciCatena::cCommandStream::CommandFn cmdProvision;

    McciCatena::cCommandStream::CommandStatus cmdProvision(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        );

Description:
    The "provision" command used to switch from normal mode to LPTIM mode.
	LPTIM mode is used to reduce the power consumption.

Returns:
    cCommandStream::CommandStatus::kSuccess if successful.
    Some other value for failure.

*/

static bool parseUint32(
    const char *pValue,
    size_t nValue,
    std::uint32_t &result,
    size_t &nParsed
    )
    {
    if (nValue == 0)
        {
        nParsed = 0;
        return false;
        }

    bool fOverflow = false;
    nParsed = McciAdkLib_BufferToUint32(pValue, nValue, 0, &result, &fOverflow);
    if (fOverflow)
        return false;

    if (nParsed != nValue)
        return false;

    return true;
    }

cCommandStream::CommandStatus cmdProvision(
    cCommandStream *pThis,
    void *pContext,
    int argc,
    char **argv
    )
    {
    std::uint32_t howLong;

    if (argc == 1)
        {
        howLong = 60;
        }
    else if (argc == 2)
        {
        std::size_t nParsed;
        if (! parseUint32(argv[1], std::strlen(argv[1]), howLong, nParsed))
            {
            pThis->printf("%s: invalid number of seconds: %s\n", argv[1]);
            return cCommandStream::kInvalidParameter;
            }
        }

    pThis->printf("LPTIM sleep disabled for %u seconds to provision\n", howLong);
    delay(100);
    start_provisioning(howLong);

    return cCommandStream::kSuccess;
    }
