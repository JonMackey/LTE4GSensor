/*
*	StringUtils.h, Copyright Jonathan Mackey 2021
*	Common string utilities.
*
*	GNU license:
*	This program is free software: you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation, either version 3 of the License, or
*	(at your option) any later version.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*	Please maintain this license information along with authorship and copyright
*	notices in any redistribution of this code.
*
*/
#ifndef StringUtils_h
#define StringUtils_h

#include <inttypes.h>

class StringUtils
{
public:
	static char				SkipWhitespaceOnLine(
								const char*&			ioBufferPtr);
	static bool				SkipTillChar(
								char					inChar,
								bool					inSkipCharToo,
								const char*&			ioBufferPtr);
	static char				SkipToNextLine(
								const char*&			ioBufferPtr);
	static char				GetUInt16Value(
								const char*&			ioBufferPtr,
								uint16_t& 				outValue);
	static char				GetInt16Value(
								const char*&			ioBufferPtr,
								int16_t& 				outValue);
	static bool				CmpBufferP(
								const char*				inCmpStr,
								char*&					ioBufferPtr);
	static char*			Uint8ToHexStr(
								uint8_t					inNum,
								char*					inBufferPtr);
	static uint8_t			HexStrToUint8(
								const char*&			ioBufferPtr);
	static void				Uint16ToDecStr(
								uint16_t				inNum,
								char*					inBuffer);
	static uint8_t			Fixed16ToDec10Str(
								int16_t					inNum,
								char*					inBuffer);
	static inline uint8_t	HexAsciiToBin(
								uint8_t					inByte)
								{return (inByte <= '9' ?
											(inByte - '0') :
											(inByte - ('A' - 10)));}
	static uint8_t			GetToken(
								uint8_t					inMaxLen,
								const char*				inString,
								char*					outToken);
	static uint8_t			FindTokenP(
								const char*				inToken,
								const char*	const*		inStrArray,
								uint8_t					inArrayLen);
											
	static const char kHexChars[];

};

#endif // StringUtils_h
