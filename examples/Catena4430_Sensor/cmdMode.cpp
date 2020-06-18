/*

Module: Catena4430_cmdMode.cpp

Function:
    Process the "mode" command.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Dhinesh Kumar Pitchai, MCCI Corporation   June 2020

*/

#include "Catena4430_cmd.h"

#include "Catena4430.h"

using namespace McciCatena;

extern void setup_mode(void);

/*

Name:   ::cmdMode()

Function:
    Command dispatcher for "mode" command.

Definition:
    McciCatena::cCommandStream::CommandFn cmdMode;

    McciCatena::cCommandStream::CommandStatus cmdMode(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        );

Description:
    The "mode" command used to switch from normal mode to LPTIM mode.
	LPTIM mode is used to reduce the power consumption.

Returns:
    cCommandStream::CommandStatus::kSuccess if successful.
    Some other value for failure.

*/

cCommandStream::CommandStatus cmdMode(
    cCommandStream *pThis,
    void *pContext,
    int argc,
    char **argv
    )
    {
    if (argc == 1)
        {
        pThis->printf("Mode change in process\n");
        delay(100);
        setup_mode();
        return cCommandStream::kSuccess;
        }
    else if (argc == 2)
        {
        pThis->printf("Ivalid command!\n");
        return cCommandStream::kInvalidParameter;
        }   
    }
