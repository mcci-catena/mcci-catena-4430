/*

Module: Catena4430_cmd.h

Function:
    Linkage for Catena4430 command processing.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   November 2019

*/

#ifndef _Catena4430_cmd_h_
#define _Catena4430_cmd_h_

#pragma once

#include <Catena_CommandStream.h>

McciCatena::cCommandStream::CommandFn cmdDate;
McciCatena::cCommandStream::CommandFn cmdLog;

#endif /* _Catena4430_cmd_h_ */
