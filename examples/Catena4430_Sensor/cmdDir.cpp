/*

Module:	cmdDir.cpp

Function:
    Print directory of SD card

Copyright and License:
    This file copyright (C) 2021 by

        MCCI Corporation
        3520 Krums Corners Road
        Ithaca, NY  14850

    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation	May 2021

*/

#include "Catena4430_cmd.h"

#include "Catena4430_Sensor.h"

using namespace McciCatena;

/****************************************************************************\
|
|   Manifest declarations
|
\****************************************************************************/

static void
printDirectory(
    cCommandStream *pThis,
    File &dir,
    unsigned level,
    bool fRecurse
    );

/*

Name:   ::cmdDir()

Function:
    Command dispatcher for "dir" command.

Definition:
    McciCatena::cCommandStream::CommandFn cmdDir;

    McciCatena::cCommandStream::CommandStatus cmdDir(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        );

Description:
    The "dir" command has the following syntax:

    dir
        Display the root directory.

    dir {path}
        Display a directory of path

    If "tree" is used instead of "dir", then a recursive
    directory listing is produced.

Returns:
    cCommandStream::CommandStatus::kSuccess if successful.
    Some other value for failure.

*/

// argv[0] is "dir"
// argv[1] is path; if omitted, root is listed
cCommandStream::CommandStatus cmdDir(
    cCommandStream *pThis,
    void *pContext,
    int argc,
    char **argv
    )
    {
    const char *sFile;

    if (argc > 2)
        return cCommandStream::CommandStatus::kInvalidParameter;

    if (argc == 1)
        sFile = "/";
    else
        sFile = argv[1];

    auto dir = gSD.open(sFile);
    if (! dir)
        {
        pThis->printf("%s: not found: %s\n", argv[0], sFile);
        return cCommandStream::CommandStatus::kReadError;
        }

    printDirectory(pThis, dir, 0, argv[0][0] == 't');
    dir.close();
    return cCommandStream::CommandStatus::kSuccess;
    }

static void
printDirectory(
    cCommandStream *pThis,
    File &dir,
    unsigned level,
    bool fRecurse
    )
    {
    while (true)
        {
        File entry = dir.openNextFile();
        if (! entry)
            return;

        pThis->printf("%.*s", 4 * level, "");
        pThis->printf("%s", entry.name());
        if (entry.isDirectory())
            {
            pThis->printf("/\n");
            if (fRecurse)
                printDirectory(pThis, entry, level + 1, fRecurse);
            }
        else
            {
            pThis->printf("%.*u\n", 16 - strlen(entry.name()) + 8, entry.size());
            }
        entry.close();
        }
    }
