/*
*	TPDU.h
*	Copyright (c) 2021 Jonathan Mackey
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
*/
#ifndef TPDU_H
#define TPDU_H

#include "StringUtils.h"

class TPDU : public StringUtils
{
public:
	typedef char	TPAddress[16];
	static uint8_t			CreateSMSSubmitPDU(
								const char*				inPhoneNumber,
								const char*				inMessageStr,
								bool					inIsDomesticPhoneNumber,
								char*					outTPDU);
	static uint8_t			ExtractAddress(
								const char*				inBuffer,
								bool					inIsSMSC,
								TPAddress&				outAddress,
								uint8_t*				outFormat = nullptr);
	static uint8_t			ParseTPDU(
								const char*				inBuffer,
								char*					outMessage,	// Must be at least 140 characters.
								TPAddress&				outSender);

	static uint8_t			DecStrToSemiOctetStr(
								const char*				inDecimalStr,
								char*					outSemiOctetStr,
								uint8_t					inMaxChars = 99);
	static char*			Pack7BitToPDU(
								const char*				in7BitStr,
								char*					outPDU);
	static uint8_t			UnpackPDUTo7bit(
								const char*				inPDU,
								char*					out7BitStr);
	static bool				SameAddress(
								const TPAddress&		inAddress1,
								const TPAddress&		inAddress2);
								
};

#endif
