/*

Module: Catena4430_cmdDate.cpp

Function:
    Process the "date" command.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   November 2019

*/

#include "Catena4430_cmd.h"

#include "Catena4430.h"
#include <Catena_Date.h>

using namespace McciCatena;

/*

Name:   ::cmdDate()

Function:
    Command dispatcher for "date" command.

Definition:
    McciCatena::cCommandStream::CommandFn cmdDate;

    McciCatena::cCommandStream::CommandStatus cmdDate(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        );

Description:
    The "date" command has the following syntax:

    date
        Display the current date.

    date yyyy-mm-dd hh:mm:ss
        Set the date and time.

    date hh:mm:ss
        Set the time only (leave date alone)

    date yyy-mm-dd
        Set the date only (leave time alone)

    The clock runs in the UTC/GMT/Z time-zone.

Returns:
    cCommandStream::CommandStatus::kSuccess if successful.
    Some other value for failure.

*/

// argv[0] is the matched command name.
// argv[1], [2] if present are the new mask date, time.
cCommandStream::CommandStatus cmdDate(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        )
        {
        bool fResult;
        cDate d;
        bool fInitialized = gClock.isInitialized();
        const char *pEnd;

        if (fInitialized)
            {
            unsigned errCode = 0;
            if (! gClock.get(d, &errCode))
                {
                fInitialized = false;
                pThis->printf("gClock.get() failed: %u\n", errCode);
                }
            }

        if (argc < 2)
            {
            if (! fInitialized)
                pThis->printf("clock not initialized\n");
            else
                pThis->printf("%04u-%02u-%02u %02u:%02u:%02uZ (GPS: %lu)\n",
                    d.year(), d.month(), d.day(),
                    d.hour(), d.minute(), d.second(),
                    static_cast<unsigned long>(d.getGpsTime())
                    );

            return cCommandStream::CommandStatus::kSuccess;
            }

        fResult = true;
        if (argc > 3)
            {
            fResult = false;
            pThis->printf("too many args\n");
            }

        if (fResult)
            {
            if (d.parseDateIso8601(argv[1]))
                {
                if (argv[2] != nullptr && ! d.parseTime(argv[2], &pEnd))
                    {
                    pThis->printf("invalid time after date: %s\n", argv[2]);
                    fResult = false;
                    }
                else if (argv[2] != nullptr)
                    {
                    if (pEnd == NULL ||
                        ! ((pEnd[0] == 'z' || pEnd[0] == 'Z') && pEnd[1] == '\0'))
                        {
                        pThis->printf("expected Z suffix to remind you it's UTC+0 (GMT) time\n");
                        fResult = false;
                        }
                    }
                }
            else if (d.parseTime(argv[1], &pEnd))
                {
                if (argv[2] != nullptr);
                    {
                    pThis->printf("nothing allowed after time: %s\n", argv[2]);
                    fResult = false;
                    }
                if (pEnd == NULL ||
                    ! ((pEnd[0] == 'z' || pEnd[0] == 'Z') && pEnd[1] == '\0'))
                    {
                    pThis->printf("expected Z suffix to remind you it's UTC+0 (GMT) time\n");
                    fResult = false;
                    }
                }
            else
                {
                pThis->printf("not a date or time: %s\n", argv[1]);
                fResult = false;
                }

            if (fResult)
                {
                unsigned errCode;
                if (! gClock.set(d, &errCode))
                    {
                    pThis->printf("couldn't set clock: %u\n", errCode);
                    return cCommandStream::CommandStatus::kIoError;
                    }
                }
            }

        return fResult ? cCommandStream::CommandStatus::kSuccess
                       : cCommandStream::CommandStatus::kInvalidParameter
                       ;
        }
