/*
*	StringUtils.cpp, Copyright Jonathan Mackey 2021
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
#include "StringUtils.h"
#ifndef __MACH__
#include <Arduino.h>
#include <avr/pgmspace.h>
#else
#include <ctype.h>
#include <string.h>
#define pgm_read_byte(xx) *(xx)
#define pgm_read_word(xx) *(xx)
#define strcmp_P strcmp
#endif

const char StringUtils::kHexChars[] = "0123456789ABCDEF";

/**************************** SkipWhitespaceOnLine ****************************/
/*
*	Same as SkipWhitespace except it doesn't treat \n as whitespace
*	On return the ioBufferPtr points to the first non-space character.
*/
char StringUtils::SkipWhitespaceOnLine(
	const char*&	ioBufferPtr)
{
	char	thisChar = *ioBufferPtr;
	for (; thisChar; thisChar = *(++ioBufferPtr))
	{
		if (thisChar != '\n' &&
			isspace(thisChar))
		{
			continue;
		}
		break;
	}
	return(thisChar);
}

/******************************** SkipTillChar ********************************/
/*
*	Skips till inChar or EOS which ever occurs first.
*	returns false if EOS is hit.
*/
bool StringUtils::SkipTillChar(
	char			inChar,
	bool			inSkipCharToo,
	const char*&	ioBufferPtr)
{
	
	char	thisChar = *ioBufferPtr;
	for (; thisChar; thisChar = *(++ioBufferPtr))
	{
		if (thisChar != inChar)
		{
			continue;
		}
		if (inSkipCharToo)
		{
			thisChar = *(++ioBufferPtr);
		}
		break;
	}
	return(thisChar != 0);
}

/******************************* SkipToNextLine *******************************/
char StringUtils::SkipToNextLine(
	const char*&	ioBufferPtr)
{
	char thisChar = *(ioBufferPtr++);
	for (; thisChar; thisChar = *(ioBufferPtr++))
	{
		if (thisChar != '\r')
		{
			if (thisChar != '\n')
			{
				continue;
			}
		} else if (*ioBufferPtr != '\n')
		{
			continue;
		}
		thisChar = *(++ioBufferPtr);	// Consume the newline, get next char
		break;
	}
	return(thisChar);
}

/******************************* GetUInt16Value *******************************/
/*
*	On exit, ioBufferPtr is advanced by the number of digits consumed.
*	ioBufferPtr points to the first non-digit character.
*	The non-digit character is returned.
*/
char StringUtils::GetUInt16Value(
	const char*&	ioBufferPtr,
	uint16_t& 		outValue)
{
	char	thisChar = *ioBufferPtr;
	uint32_t	value = 0;
	for (; isdigit(thisChar); thisChar = *(++ioBufferPtr))
	{
		value = (value * 10) + (thisChar-'0');
	}
	outValue = value;
	return(thisChar);
}

/******************************* GetInt16Value ********************************/
/*
*	On exit, ioBufferPtr is advanced by the number of digits consumed.
*	ioBufferPtr points to the first non-digit character.
*/
char StringUtils::GetInt16Value(
	const char*&	ioBufferPtr,
	int16_t& 		outValue)
{
	char	thisChar = *ioBufferPtr;
	int32_t	value = 0;
	bool	isNegative = thisChar == '-';
	if (isNegative)
	{
		thisChar = *(++ioBufferPtr);
	}
	for (; isdigit(thisChar); thisChar = *(++ioBufferPtr))
	{
		value = (value * 10) + (thisChar-'0');
	}
	outValue = isNegative ? -value:value;
	return(thisChar);
}

/********************************* CmpBufferP *********************************/
/*
*	Compares the PROGMEM string inCmpStr to the buffer.  True is returned if
*	ioBufferPtr starts with inCmpStr.  If true, the ioBufferPtr is advanced by
*	the length of inCmpStr otherwise ioBufferPtr is unchanged.
*	Note that strcmp_P isn't used because the ioBufferPtr may not be terminated,
*	the compare can be a substring.
*/
bool StringUtils::CmpBufferP(
	const char*	inCmpStr,
	char*&		ioBufferPtr)
{
	char*	buffPtr = ioBufferPtr;
	char		thisChar;
	
	// Note: The compare may go past the end of ioBufferPtr.
	
	while (true) 
	{
		thisChar = pgm_read_byte(inCmpStr++);
		if (thisChar != 0)
		{
			if (thisChar == *(buffPtr++))
			{
				continue;
			}
			break;
		}
		ioBufferPtr = buffPtr;
		return(true);
	}
	return(false);
}

/******************************* Uint8ToHexStr ********************************/
/*
*	inNum is converted to its 2 byte hex ASCII representation.
*	On exit inBufferPtr is a hex8 str with leading zeros (0x1 would return 01)
*	Returns inBufferPtr + 2.
*/
char* StringUtils::Uint8ToHexStr(
	uint8_t	inNum,
	char*	inBufferPtr)
{
	inBufferPtr[0] = kHexChars[inNum>>4];
	inBufferPtr[1] = kHexChars[inNum & 0xF];
	return(inBufferPtr+2);
}

/******************************* HexStrToUint8 ********************************/
/*
*	Consumes 2 Hex ASCII bytes of ioBufferPtr and returns its value.
*	On exit ioBufferPtr is advanced by 2 bytes.
*	No sanity check is done on the values that ioBufferPtr points to.
*/
uint8_t StringUtils::HexStrToUint8(
	const char*&	ioBufferPtr)
{
	uint8_t	hexNum0 = ioBufferPtr[0];
	uint8_t	hexNum1 = ioBufferPtr[1];
	ioBufferPtr+=2;
	return (((hexNum0 - (hexNum0 <= '9' ? '0' : ('A'-10))) << 4)
					+ (hexNum1 - (hexNum1 <= '9' ? '0' : ('A'-10))));
}

/******************************* Uint16ToDecStr *******************************/
void StringUtils::Uint16ToDecStr(
	uint16_t	inNum,
	char*		inBuffer)
{
	for (uint16_t num = inNum; num/=10; inBuffer++);
	inBuffer[1] = 0;
	do
	{
		*(inBuffer--) = (inNum % 10) + '0';
		inNum /= 10;
	} while (inNum);
}

/***************************** Fixed16ToDec10Str ******************************/
/*
*	inNum is a 16 bit fixed-point with 1/16 scale.
*	Result is a decimal string with 1 decimal place accuracy.
*
*	Returns the number of characters before the decimal point.  This is used for
*	text that you want to center on the decimal point so it doesn't jump around.
*	This is most commonly noticeable for celsius around 0C
*/
uint8_t StringUtils::Fixed16ToDec10Str(
	int16_t	inNum,
	char*	inBuffer)
{
	uint8_t	charsBeforeDec = 0;
	if (inNum < 0)
	{
		*(inBuffer++) = '-';
		inNum = -inNum;
		charsBeforeDec++;
	}
	int32_t	decNum = inNum >> 4;
	for (int32_t num = decNum; num/=10; inBuffer++);
	char*	bufPtr = inBuffer;
	do
	{
		*(bufPtr--) = (decNum % 10) + '0';
		decNum /= 10;
	} while (decNum);
	charsBeforeDec += (inBuffer - bufPtr);
	inBuffer[1] = '.';
	// Always round up, even for negative numbers.
	// Convert 1/16 fixed-point fraction to 1/1000, round up, then convert to 1/1
	inBuffer[2] = ((((inNum & 0xF)*625)+500) / 1000) + '0';
	inBuffer[3] = 0;
	return(charsBeforeDec);
}

/********************************** GetToken **********************************/
/*
*	Scans up to inMaxLen characters of inString looking for the first non-alpha
*	character or null, whichever occurs first.
*	The returned token will be converted to lowercase as needed.
*/
 uint8_t StringUtils::GetToken(
	uint8_t		inMaxLen,	// Not including the null terminator
	const char*	inString,
	char*		outToken)
{
	char	thisChar = *inString;
	uint8_t	tokenLen = 0;
	for (; thisChar; thisChar = *(++inString))
	{
		if (!isspace(thisChar) &&
			tokenLen < inMaxLen)
		{
			outToken[tokenLen] = tolower(thisChar);
			tokenLen++;
			continue;
		}
		break;
	}
	outToken[tokenLen] = 0;
	return(tokenLen);
}

/********************************* FindTokenP *********************************/
/*
*	Performs a linear search for inToken within inStrArray (stored in PROGMEM.)
*	Returns zero if the token wasn't found.
*	Returns the token index + 1 if found.
*/
uint8_t StringUtils::FindTokenP(
	const char*			inToken,
	const char*	const*	inStrArray,
	uint8_t				inArrayLen)
{
	uint8_t		tokenIndex = inArrayLen;
	const char*	stringPtr;
	while (tokenIndex)
	{
		tokenIndex--;
		stringPtr = (const char*)pgm_read_word(&inStrArray[tokenIndex]);
		if (strcmp_P(inToken, stringPtr) != 0)
		{
			continue;
		}
		return(tokenIndex+1);
	}
	return(0);
}
