/*
* This file is part of the Pandaria 5.4.8 Project. See THANKS file for Copyright information
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "ByteBuffer.h"
#include "Errors.h"
#include "Common.h"
#include "Log.h"
#include <utf8.h>
#include <sstream>
#include <ctime>
#include "StringFormat.h"
#include "Util.h"

ByteBufferPositionException::ByteBufferPositionException(bool add, size_t pos,
                                                         size_t size, size_t valueSize)
{
    std::ostringstream ss;

    ss << "Attempted to " << (add ? "put" : "get") << " value with size: "
       << valueSize << " in ByteBuffer (pos: " << pos << " size: " << size
       << ")";

    message().assign(ss.str());
}

ByteBufferSourceException::ByteBufferSourceException(size_t pos, size_t size,
                                                     size_t valueSize)
{
    std::ostringstream ss;

    ss << "Attempted to put a "
       << (valueSize > 0 ? "NULL-pointer" : "zero-sized value")
       << " in ByteBuffer (pos: " << pos << " size: " << size << ")";

    message().assign(ss.str());
}

ByteBufferInvalidValueException::ByteBufferInvalidValueException(char const* type, char const* value)
{
    message().assign(Trinity::StringFormat("Invalid %s value (%s) found in ByteBuffer", type, value));
}

void ByteBuffer::clear()
{
    _storage.clear();
    _rpos = _wpos = 0;
}

// template <typename T> 
// void ByteBuffer::append(T value)
// {
//     FlushBits();
//     EndianConvert(value);
//     append((uint8 *)&value, sizeof(value));
// }

void ByteBuffer::FlushBits()
{
    if (_bitpos == 8)
        return;

    append((uint8 *)&_curbitval, sizeof(uint8));
    _curbitval = 0;
    _bitpos = 8;    
}

void ByteBuffer::WriteBitInOrder(ObjectGuid guid, uint8 order[8])
{
    for (uint8 i = 0; i < 8; ++i)
        WriteBit(guid[order[i]]);    
}

std::string ByteBuffer::ReadCString(bool requireValidUtf8 /*= true*/)
{
    std::string value;
    while (rpos() < size())                         // prevent crash at wrong string format in packet
    {
        char c = read<char>();
        if (c == 0)
            break;
        value += c;
    }
    if (requireValidUtf8 && !utf8::is_valid(value.begin(), value.end()))
        throw ByteBufferInvalidValueException("string", value.c_str());
    return value;
}

uint32 ByteBuffer::ReadPackedTime()
{
    uint32 packedDate = read<uint32>();
    tm lt = tm();

    lt.tm_min = packedDate & 0x3F;
    lt.tm_hour = (packedDate >> 6) & 0x1F;
    //lt.tm_wday = (packedDate >> 11) & 7;
    lt.tm_mday = ((packedDate >> 14) & 0x3F) + 1;
    lt.tm_mon = (packedDate >> 20) & 0xF;
    lt.tm_year = ((packedDate >> 24) & 0x1F) + 100;

    return uint32(mktime(&lt));
}

void ByteBuffer::append(uint8 const* src, size_t cnt)
{
    if (!cnt)
        throw ByteBufferSourceException(_wpos, size(), cnt);

    if (!src)
        throw ByteBufferSourceException(_wpos, size(), cnt);

    ASSERT(size() < 10000000);

    size_t const newSize = _wpos + cnt;
    if (_storage.capacity() < newSize) // custom memory allocation rules
    {
        if (newSize < 100)
            _storage.reserve(300);
        else if (newSize < 750)
            _storage.reserve(2500);
        else if (newSize < 6000)
            _storage.reserve(10000);
        else
            _storage.reserve(400000);
    }

    if (_storage.size() < _wpos + cnt)
        _storage.resize(_wpos + cnt);
    std::memcpy(&_storage[_wpos], src, cnt);
    _wpos += cnt;
}

// void ByteBuffer::append(uint8 const* src, size_t cnt)
// {
//     ASSERT(src, "Attempted to put a NULL-pointer in ByteBuffer (pos: " SZFMTD " size: " SZFMTD ")", _wpos, size());
//     ASSERT(cnt, "Attempted to put a zero-sized value in ByteBuffer (pos: " SZFMTD " size: " SZFMTD ")", _wpos, size());
//     ASSERT(size() < 10000000);

//     size_t const newSize = _wpos + cnt;
//     if (_storage.capacity() < newSize) // custom memory allocation rules
//     {
//         if (newSize < 100)
//             _storage.reserve(300);
//         else if (newSize < 750)
//             _storage.reserve(2500);
//         else if (newSize < 6000)
//             _storage.reserve(10000);
//         else
//             _storage.reserve(400000);
//     }

//     if (_storage.size() < newSize)
//         _storage.resize(newSize);
//     std::memcpy(&_storage[_wpos], src, cnt);
//     _wpos = newSize;
// }

void ByteBuffer::put(size_t pos, const uint8 *src, size_t cnt)
{
    if (pos + cnt > size())
        throw ByteBufferPositionException(true, pos, cnt, size());

    if (!src)
        throw ByteBufferSourceException(_wpos, size(), cnt);

    std::memcpy(&_storage[pos], src, cnt);
}

void ByteBuffer::print_storage() const
{
    if (!sLog->ShouldLog("network", LOG_LEVEL_TRACE)) // optimize disabled trace output
        return;

    std::ostringstream o;
    o << "STORAGE_SIZE: " << size();
    for (uint32 i = 0; i < size(); ++i)
        o << read<uint8>(i) << " - ";
    o << " ";

    TC_LOG_TRACE("network", "%s", o.str().c_str());
}

void ByteBuffer::textlike() const
{
    if (!sLog->ShouldLog("network", LOG_LEVEL_TRACE)) // optimize disabled trace output
        return;

    std::ostringstream o;
    o << "STORAGE_SIZE: " << size();
    for (uint32 i = 0; i < size(); ++i)
    {
        char buf[2];
        snprintf(buf, 2, "%c", read<uint8>(i));
        o << buf;
    }
    o << " ";
    TC_LOG_TRACE("network", "%s", o.str().c_str());
}

void ByteBuffer::hexlike() const
{
    if (!sLog->ShouldLog("network", LOG_LEVEL_TRACE)) // optimize disabled trace output
        return;

    uint32 j = 1, k = 1;

    std::ostringstream o;
    o << "STORAGE_SIZE: " << size();

    for (uint32 i = 0; i < size(); ++i)
    {
        char buf[4];
        snprintf(buf, 4, "%2X ", read<uint8>(i));
        if ((i == (j * 8)) && ((i != (k * 16))))
        {
            o << "| ";
            ++j;
        }
        else if (i == (k * 16))
        {
            o << "\n";
            ++k;
            ++j;
        }

        o << buf;
    }
    o << " ";
    TC_LOG_TRACE("network", "%s", o.str().c_str());
}

void ByteBuffer::AppendPackedTime(time_t time)
{
    tm lt;
    localtime_r(&time, &lt);
    append<uint32>((lt.tm_year - 100) << 24 | lt.tm_mon << 20 | (lt.tm_mday - 1) << 14 | lt.tm_wday << 11 | lt.tm_hour << 6 | lt.tm_min);
}