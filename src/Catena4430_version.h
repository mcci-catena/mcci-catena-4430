/*

Module: Catena4430_version.h

Function:
    The Catena4430 library: the version constants

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   August 2019

*/

#ifndef _Catena4430_version_h_
# define _Catena4430_version_h_

#pragma once

#include <cstdint>

namespace McciCatena4430 {

// create a version number for comparison
static constexpr std::uint32_t
makeVersion(
    std::uint8_t major, std::uint8_t minor, std::uint8_t patch, std::uint8_t local = 0
    )
    {
    return ((std::uint32_t)major << 24u) | ((std::uint32_t)minor << 16u) | ((std::uint32_t)patch << 8u) | (std::uint32_t)local;
    }

// extract major number from version
static constexpr std::uint8_t
getMajor(std::uint32_t v)
    {
    return std::uint8_t(v >> 24u);
    }

// extract minor number from version
static constexpr std::uint8_t
getMinor(std::uint32_t v)
    {
    return std::uint8_t(v >> 16u);
    }

// extract patch number from version
static constexpr std::uint8_t
getPatch(std::uint32_t v)
    {
    return std::uint8_t(v >> 8u);
    }

// extract local number from version
static constexpr std::uint8_t
getLocal(std::uint32_t v)
    {
    return std::uint8_t(v);
    }

// version of library, for use by clients in static_asserts
static constexpr std::uint32_t kVersion = makeVersion(0,5,2,0);

} // namespace McciCatena4430

#endif // ndef _Catena4430_version_h_
