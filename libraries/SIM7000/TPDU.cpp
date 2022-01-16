/*
*	TPDU.cpp
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
#include "TPDU.h"
#ifndef __MACH__
#include <Arduino.h>
#else
#include <stdio.h>
#include <string.h>
#endif

/***************************** CreateSMSSubmitPDU *****************************/
/*
*	Creates an SMS Submit TPDU (Transport Protocol Data Units) string, suitable
*	for use with CMGS (send SMS message) when CMGF (select SMS message format)
*	is set to PDU (0) rather than Text (1).
*
*	The result is written to outTPDU.
*	The number of octets is returned (does not include the SMSC, as per doc)
*
*	https://en.wikipedia.org/wiki/GSM_03.40
*/
uint8_t TPDU::CreateSMSSubmitPDU(
	const char*	inPhoneNumber,
	const char*	inMessageStr,
	bool		inIsDomesticPhoneNumber,
	char*		outTPDU)
{
	char*	txBufferPtr = outTPDU;
	
	// Example of specifying the SMSC.  Rather than the -2 in the return, it
	// should be -16, the length of the example SMSC.
	//strcpy(txBufferPtr, "07919130364886F2");
	//txBufferPtr+=16;
	
	txBufferPtr = Uint8ToHexStr(0, txBufferPtr);	// Use the default SMSC
	txBufferPtr = Uint8ToHexStr(0x11, txBufferPtr);	// SMS-SUBMIT + Relative validity period
	txBufferPtr = Uint8ToHexStr(0, txBufferPtr);	// Default encoding
	uint8_t	phoneLen = DecStrToSemiOctetStr(inPhoneNumber, &txBufferPtr[4]);
	txBufferPtr = Uint8ToHexStr(phoneLen, txBufferPtr);	// Destination Phone number length
	txBufferPtr = Uint8ToHexStr(inIsDomesticPhoneNumber ? 0x81:0x91, txBufferPtr);	// Destination number type
	txBufferPtr += ((phoneLen+1) & 0xFE);	// Skip the destination phone number
	txBufferPtr = Uint8ToHexStr(0, txBufferPtr);	// Protocol Normal
	txBufferPtr = Uint8ToHexStr(0, txBufferPtr);	// Data Coding Scheme, GSM 7 bit
	txBufferPtr = Uint8ToHexStr(0xA7, txBufferPtr); // Validity period 1 day
	
	// The -2 below is to exclude the SMSC from the number of octets in
	// the TPDU.
	return((Pack7BitToPDU(inMessageStr, txBufferPtr)-outTPDU-2)/2);
}

/**************************** DecStrToSemiOctetStr ****************************/
/*
*	Note that the string returned is not nul terminated.
*	To calculate the actual end of the string offset, add 1 to the length value
*	returned and AND it with 0xFE, as done in CreateSMSSubmitPDU().
*	Ex: 15189723132 is 11 bytes and would convert to the 12 byte string
*	    5181793231F2.  The length of 12 is calculated as (11+1) & 0xFE = 12
*/
uint8_t TPDU::DecStrToSemiOctetStr(
	const char*		inDecimalStr,
	char*			outSemiOctetStr,
	uint8_t			inMaxChars)
{
	uint8_t	decIndex = 0;
	uint8_t	semiOctetIndex = 0;

	uint8_t	decVal;
	for (decVal = inDecimalStr[decIndex]; decVal && inMaxChars; decVal = inDecimalStr[decIndex], inMaxChars--)
	{
		if (decIndex & 1)
		{
			outSemiOctetStr[semiOctetIndex] = decVal;
			semiOctetIndex+=2;
		} else
		{
			outSemiOctetStr[semiOctetIndex+1] = decVal;
		}
		decIndex++;
	}
	if (decIndex & 1)
	{
		outSemiOctetStr[semiOctetIndex] = 'F';
	}
	return(decIndex);
}

/********************************* ParseTPDU **********************************/
/*
*	On entry inBuffer points to the first octet of the TPDU record.
*	Currently this only supports SMS DELIVER (message type 00)
*	Returns the length of the message
*/
uint8_t TPDU::ParseTPDU(
	const char*	inBuffer,
	char*		outMessage,	// Must be at least 140 characters.
	TPAddress&	outSender)	// Can be nil
{
	/*
	struct SFirstOctetDeliver
	{
		uint8_t	messageTypeInd : 2;		// mask 0x03
		uint8_t moreToSend : 1;			// mask 0x04
		uint8_t	loopPrevention : 1;		// mask 0x08
		uint8_t	unused : 1;
		uint8_t statusReportReq : 1;	// mask 0x20
		uint8_t	userDataHdrInd : 1;		// mask 0x40
		uint8_t	replyPath : 1;			// mask 0x80
	};
	struct SFirstOctetSubmit
	{
		uint8_t	messageTypeInd : 2;		// mask 0x03
		uint8_t rejectDuplicates : 1;	// mask 0x04
		uint8_t	validityPeriodFmt : 2;	// mask 0x18
		uint8_t statusReportInd : 1;	// mask 0x20
		uint8_t	userDataHdrInd : 1;		// mask 0x40
		uint8_t	replyPath : 1;			// mask 0x80
	};
	union UFirstOctet
	{
		struct SFirstOctetDeliver deliver;
		struct SFirstOctetSubmit submit;
		uint8_t	acc;
	} firstOctet;*/
	//firstOctet.acc = HexStrToUint8(inBuffer);
	const char*	bufferPtr = inBuffer;
	uint8_t	messageType = HexStrToUint8(bufferPtr) & 3;
	uint8_t	messageLen = 0;
	/*
	*	If this is message type SMS DELIVER...
	*/
	if (messageType == 0)
	{
		// Get the Originating Address
		bufferPtr += ExtractAddress(bufferPtr, false, outSender, nullptr);
		// Skip the protocol + data coding scheme + timestamp
		bufferPtr += (2+2+14);
		messageLen = UnpackPDUTo7bit(bufferPtr, outMessage);
	}
	return(messageLen);
}

/******************************* ExtractAddress *******************************/
/*
*	Extracts either the SMSC address or the originating/destination/recipient
*	TPDU address.
*	Optionally returns the format and address from an SMS encoded message.
*	Returns the length in bytes of the record (used to advance the buffer ptr.)
*	inIsSMSC determines how the record length is calculated.  For TPDU addresses
*	the length is the value rounded up to the nearest even int + 4.  For SMSC
*	the length is the (value*2)+2
*
*	On entry the inBuffer points to start of the record.
*/
uint8_t TPDU::ExtractAddress(
	const char*	inBuffer,
	bool		inIsSMSC,	// Else TPDU
	TPAddress&	outAddress,
	uint8_t*	outFormat)
{
	/*
	*	The record is made up of the following fields:
	*	[2] length For SMSC the length is either the number of octets or the
	*	format and the number combined.  For a non-SMSC address in the TPDU the
	*	length is the actual length of the address.
	*	[2] Address format, 0x91 or 0x81, international or domestic
	*	[Depends on inIsSMSC] address
	*/
	uint8_t	recordLen = HexStrToUint8(inBuffer);
	if (inIsSMSC)
	{
		recordLen += (recordLen + 2);
	// Else it's a non-SMSC address in the TPDU
	} else
	{
		recordLen += ((recordLen & 1) + 4);	// Rounded to nearest even value +4.
	}
	if (recordLen)
	{
		if (outFormat)
		{
			*outFormat = HexStrToUint8(inBuffer);
		} else
		{
			inBuffer += 2;
		}
		if (outAddress)
		{
			// DecStrToSemiOctetStr will swap the bytes back but doesn't deal with
			// the odd byte padding.
			uint8_t	addressLen = DecStrToSemiOctetStr(inBuffer, outAddress, recordLen-4);
			/*
			*	If the last byte is padding...
			*/
			if (outAddress[addressLen-1] == 'F')
			{
				addressLen--;
			}
			outAddress[addressLen] = 0;
			addressLen++;	// For the calc below, include the terminator
			if (addressLen < sizeof(TPAddress))
			{
				memset(&outAddress[addressLen], 0xFF, sizeof(TPAddress)-addressLen);
			}
		}
	}
	return(recordLen);
}

/******************************* Pack7BitToPDU ********************************/
char* TPDU::Pack7BitToPDU(
	const char*	in7BitStr,
	char*		outPDU)
{
	/*
	*	The GSM 7-bit default encoding is not converted from UTF8 encoding
	*	to GSM 7-bit.  In addition, even if the passed string was converted, the
	*	GSM 7-bit encoding includes null, which is interpreted as the end of the
	*	C string, not the expected '@' character.  In order for this to work
	*	you'd need to pass a length param rather than rely on a null as it does
	*	now.
	*
	*	Characters that need to be escaped are not supported:
	*			   (UTF8)
	*	\f= 1B 0A	(0C)
	*	^ = 1B 14	(5E)
	*	{ = 1B 28	(7B)
	*	} = 1B 29	(7D)
	*	\ = 1B 2F	(5C)
	*	[ = 1B 3C	(5B)
	*	~ = 1B 3D	(7E)
	*	] = 1B 3E	(5D)
	*	| = 1B 40	(7C)
	*	€ = 1B 65 	(E2 82 AC)
	*
	*	Non-ascii character codes are not supported:
	*	@ = 00 (40)
	*	£ = 01 (C2 A3)
	*	$ = 02 (24)
	*	¥ = 03 (C2 A5)
	*	è = 04 (C3 A8)
	*	é = 05 (C3 A9)
	*	ù = 06 (C3 B9)
	*	ì = 07 (C3 AC)
	*	ò = 08 (C3 B2)
	*	Ç = 09 (C3 87)
	*	Ø = 0B (C3 98)
	*	ø = 0C (C3 B8)
	*	Å = 0E (C3 85)
	*	å = 0F (C3 A5)
	*	Δ = 10 (CE 94)
	*	_ = 11 (5F)
	*	Φ = 12 (CE A6)
	*	Γ = 13 (CE 93)
	*	Λ = 14 (CE 9B)
	*	Ω = 15 (CE A9)
	*	Π = 16 (CE A0)
	*	Ψ = 17 (CE A8)
	*	Σ = 18 (CE A3)
	*	Θ = 19 (CE 98)
	*	Ξ = 1A (CE 9E)
	*	Æ = 1C (C3 86)
	*	æ = 1D (C3 A6)
	*	ß = 1E (C3 9F)
	*	É = 1F (C3 89)
	*	¤ = 24 (C2 A4)
	*	¡ = 40 (C2 A1)
	*	Ä = 5B (C3 84)
	*	Ö = 5C (C3 96)
	*	Ñ = 5D (C3 91)
	*	Ü = 5E (C3 9C)
	*	§ = 5F (C2 A7)
	*	¿ = 60 (C2 BF)
	*	ä = 7B (C3 A4)
	*	ö = 7C (C3 B6)
	*	ñ = 7D (C3 B1)
	*	ü = 7E (C3 BC)
	*	à = 7F (C3 A0)
	*/
	const char*	strPtr = in7BitStr;
	char*	pduPtr = &outPDU[2];	// Skip the length param
	uint8_t	octet = *(strPtr++);
	uint8_t	shift = 0;
	uint8_t septet;

	if (octet)
	{
		while (true)
		{
			septet = *(strPtr++);
			if (septet)
			{
				pduPtr = Uint8ToHexStr(octet + (septet<<(7-shift)), pduPtr);
			} else
			{
				if (octet)
				{
					pduPtr = Uint8ToHexStr(octet, pduPtr);
				} else
				{
					strPtr--;
				}
				break;
			}
			shift++;
			if (shift < 7)
			{
				octet = septet >> shift;
			} else
			{
				shift = 0;
				octet = *(strPtr++);
				if (octet)
				{
					continue;
				}
				break;
			}
		}
	}
	Uint8ToHexStr((uint8_t)(strPtr-in7BitStr)-1, outPDU);
	*pduPtr = 0;
	return(pduPtr);
}

/****************************** UnpackPDUTo7bit *******************************/
/*
*	Returns the length of the data (i.e. message)
*/
uint8_t TPDU::UnpackPDUTo7bit(
	const char*	inPDU,
	char*		out7BitStr)
{
	uint8_t	dataLen = HexStrToUint8(inPDU);
	uint8_t	bytesLeft = dataLen;

	if (bytesLeft)
	{
		uint8_t	shift = 0;
		uint8_t	octet = HexStrToUint8(inPDU);
		uint8_t septet = octet;
		while (true)
		{
			*(out7BitStr++) = septet & 0x7F;
			bytesLeft--;
			if (bytesLeft)
			{
				septet = octet >> (7 - shift);
				if (shift < 6)
				{
					shift++;
					octet = HexStrToUint8(inPDU);
					septet += (octet << shift);
					continue;
				} else
				{
					*(out7BitStr++) = (octet >> 1) & 0x7F;
					bytesLeft--;
					if (bytesLeft)
					{
						septet = octet = HexStrToUint8(inPDU);
						shift = 0;
						continue;
					}
				}
			}
			break;
		}
	}
	*out7BitStr = 0;
	return(dataLen);
}

/******************************** SameAddress *********************************/
/*
*	Returns true if the addresses are the same.
*	This will strip off the trunk prefix (USA only, 1)
*	This is needed because some numbers that are returned in the PDU have no
*	trunk prefix, and others for the same number do.
*	I haven't investigated how to handle international numbers.
*/
bool TPDU::SameAddress(
	const TPAddress&	inAddress1,
	const TPAddress&	inAddress2)
{
	const char*	addr1 = inAddress1;
	const char*	addr2 = inAddress2;
	if (addr1[0] == '1')addr1++;
	if (addr2[0] == '1')addr2++;
	return(strncmp(addr1, addr2, sizeof(TPAddress)) == 0);
}
