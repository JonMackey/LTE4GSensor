/*
*	SIM7000.cpp
*	Copyright (c) 2022 Jonathan Mackey
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
#include <Arduino.h>
#include "SIM7000.h"
#include "SIM7000ATCmdHash.h"
#include "UnixTime.h"
#include "StringUtils.h"

/*
NETLIGHT pin status			Module status
64ms ON, 800ms OFF			No registered network
64ms ON, 3000ms OFF			Registered network
64ms ON, 300ms OFF			Data transmit
OFF							Power off or PSM mode

The SIM7000 default when new is to use autobaud.  The SIM7000 autobaud is
looking for any string with the "AT" prefix.  This can take several retries till
it goes through the predefined set of autobaud rates: 9600, 19200, 38400, 57600
and 115200 bps. Once the SIM7000 establishes the baud rate, the AT command to
set the baud rate should be sent to the SIM7000.  This reduces the startup time.

*/
//const uint32_t	SIM7000::kBaudRate = 115200;
//const uint32_t	SIM7000::kBaudRate = 19200;
const uint32_t	SIM7000::kBaudRate = 9600;
const uint8_t	kAutobaudEchoRetries = 10;

// The -1 below allows for the last byte to always be a nul.
const uint16_t	SIM7000::kRxBufferSize = SIM7000_RX_BUFFER_SIZE-1;
const uint16_t	SIM7000::kTxBufferSize = SIM7000_RX_BUFFER_SIZE-1;

#define USE_PDU_SMS_FORMAT	1

const char kERunningStr[] PROGMEM = "eRunning";
const char kEWakingUpStr[] PROGMEM = "eWakingUp";
const char kESleepingStr[] PROGMEM = "eSleeping";
const char kEGoingToSleepStr[] PROGMEM = "eGoingToSleep";

const char kEReadyStr[] PROGMEM = "eReady";
const char kEBusyStr[] PROGMEM = "eBusy";
const char kETimeoutStr[] PROGMEM = "eTimeout";
const char kEErrorStr[] PROGMEM = "eError";

const char* const kSleepStateNames[] PROGMEM =
{
	kERunningStr,
	kEWakingUpStr,
	kEGoingToSleepStr,
	kESleepingStr
};

const char* const kCommandStateNames[] PROGMEM =
{
	kEReadyStr,
	kEBusyStr,
	kETimeoutStr,
	kEErrorStr
};

#if 0
struct SCommandDesc
{
	const char*	cmdStr;
	uint16_t	timeout;
};

const SCommandDesc kCommandDesc[] PROGMEM =
{
	{kTurnOffEchoStr, 1000},
	{kSetBaudRateStr, 500},
};

const char kENoCommandStr[] PROGMEM = "eNoCommand";
const char kETurnOffEchoStr[] PROGMEM = "eTurnOffEcho";
const char kESetBaudRateStr[] PROGMEM = "eSetBaudRate";
const char kESendSMSSetPhoneNumStr[] PROGMEM = "eSendSMSSetPhoneNum";
const char kESendSMSStr[] PROGMEM = "eSendSMS";
const char kEPowerDownStr[] PROGMEM = "ePowerDown";
const char kEPowerUpStr[] PROGMEM = "ePowerUp";
const char kEUnknownCommandStr[] PROGMEM = "eUnknownCommand";

const char* const kCommandNames[] PROGMEM =
{
	kENoCommandStr,
	kETurnOffEchoStr,
	kESetBaudRateStr,
	kESendSMSSetPhoneNumStr,
	kESendSMSStr,
	kEPowerDownStr,
	kEPowerUpStr,
	kEUnknownCommandStr
};
#endif

/********************************* SIM7000 ***********************************/
SIM7000::SIM7000(
	HardwareSerial&	inSerial,
	uint8_t			inRxPin,
	uint8_t			inTxPin,
	uint8_t			inPowerPin,
	uint8_t			inResetPin)
	: mSerial(inSerial), mRxPin(inRxPin), mTxPin(inTxPin),
		mPowerPin(inPowerPin), mResetPin(inResetPin),
		mPassthrough(nullptr), mCommandState(eReady),
		mBars(0), mPendingMessagesHead(0), mPendingMessagesTail(0),
		mWaitingToProcessMessage(0), mWaitingToDeleteMessage(0),
		mDeleteMessagesAfterRead(true), mTimeIsValid(false)

{
}

/******************************* FlushRxBuffer ********************************/
void SIM7000::FlushRxBuffer(void)
{
	mRxBufferPtr = mRxBuffer;
}

/*********************************** PauseRx **********************************/
void SIM7000::PauseRx(void)
{
	if (!mRxPaused)
	{
		mRxPaused = true;
		mSerial.write(0x13);
	}
}

/********************************** ResumeRx **********************************/
void SIM7000::ResumeRx(void)
{
	if (mRxPaused)
	{
		mRxPaused = false;
		mSerial.write(0x11);
	}
}

/*********************************** begin ************************************/
void SIM7000::begin(void)
{	
	mRxBuffer[kRxBufferSize] = 0;
	/*
	*	The power and reset pins behave like open collector. Setting either to a
	*	high state allows the corresponding pin on the SIM7000 to be pulled high
	*	by a diode in the SIM7000. To isolate these 1.8V pins from the board's
	*	3.3V pins, both power and reset have a signal diode between the mcu pin
	*	and the SIM7000 pin. Setting either mcu pin to a low state will pull the
	*	corresponding SIM7000 pin low.
	*/

	mSerial.begin(kBaudRate);
	pinMode(mPowerPin, OUTPUT);
	digitalWrite(mPowerPin, HIGH);
	pinMode(mResetPin, OUTPUT);
	digitalWrite(mResetPin, HIGH);
	mSMSStatus = eSMSIdle;

	WakeUp();
}

/*********************************** Sleep ************************************/
void SIM7000::Sleep(void)
{
	if (digitalRead(mRxPin))
	{
		FlushRxBuffer();
		digitalWrite(mPowerPin, LOW);	// Put the SIM7000 module to sleep by
		mPinPeriod.Set(1200);			// keeping the power pin low for 1.2s, 
		mPinPeriod.Start();				// as per doc
		mSleepState = eGoingToSleep;
		mCheckLevelsPeriod.Set(0);
		mBars = 0;
		mBatteryLevel = 0;
	}
}

/*********************************** WakeUp ***********************************/
void SIM7000::WakeUp(void)
{
	FlushRxBuffer();
	/*
	*	The mRxPin pin state is LOW if the module is powered down and HIGH when
	*	it's powered up.  It can be powered up when the board is reset, such as
	*	when loading software or pressing the reset button on the board.
	*/
	if (digitalRead(mRxPin))
	{
		mCommandState = eReady;
		mSleepState = eRunning;
		CheckLevels();
	} else
	{
		digitalWrite(mPowerPin, LOW);	// Wake up the SIM7000 module by
		mPinPeriod.Set(1000);			// keeping the power pin low for 1s, 
		mPinPeriod.Start();				// as per doc
		mSleepState = eWakingUp;
	}
}

/*********************************** Reset ************************************/
void SIM7000::Reset(void)
{
	FlushRxBuffer();
	digitalWrite(mResetPin, LOW);	// Reset the SIM7000 module by keeping the 
	mPinPeriod.Set(250);			// reset pin low for 250ms.  The doc says
	mPinPeriod.Start();				// typical is 100ms but 100 ms does nothing.
	mSleepState = eWakingUp;
}

/*********************************** Update ***********************************/
/*
*	Called from loop() just after the layout has updated.  Anything that needs
*	time is handled here.
*/
void SIM7000::Update(void)
{
	if (mPinPeriod.Passed())
	{
		mPinPeriod.Set(0);	// Disable mPinPeriod (Passed will return false)
		digitalWrite(mPowerPin, HIGH);
		digitalWrite(mResetPin, HIGH);
		if (mSleepState == eWakingUp)
		{
			// If waking up (or resetting) the expected response should be RDY
			// followed by OK
			mCommandTimeout.Set(7000);	// 7 seconds (doc says 6.9 max)
			mCommandTimeout.Start();
		}
	}
	
	if (mWaitingToDeleteMessage &&
		ClearToSendSMS())
	{
		char	commandStr[50];
		strcpy_P(commandStr, PSTR("AT+CMGD="));
		Uint16ToDecStr(mWaitingToDeleteMessage-1, &commandStr[8]);
		if (SendCommand(commandStr, 0, 5000))	// Max time 5s as per doc
		{
			mWaitingToDeleteMessage = 0;
			//Serial.print(F("Rst W2Del = 0"));
		}
	/*
	*	Else if any messages were received THEN
	*	read the oldest message first.
	*/
	} else if (!mWaitingToProcessMessage &&
		mPendingMessagesHead != mPendingMessagesTail &&
		ClearToSend())
	{
		char	commandStr[50];
		strcpy_P(commandStr, PSTR("AT+CMGR="));
		uint8_t	messageIndex = mPendingMessages[mPendingMessagesHead];
		Uint16ToDecStr(messageIndex, &commandStr[8]);
		if (SendCommand(commandStr, 0, 5000))	// Max time 5s as per doc
		{
			mWaitingToProcessMessage = messageIndex+1;
			mPendingMessagesHead++;
			if (mPendingMessagesHead == sizeof(mPendingMessages))
			{
				mPendingMessagesHead = 0;
			}
		}
	}

	// While any characters are available from the SIM7000...
	while (mSerial.available())
	{
		uint8_t	byteRead = mSerial.read();
		switch (byteRead)
		{
			case '\r':	// Ignore carriage returns
			case 0:		// Ignore nulls
				break;
			case '\n':
				if (mPassthrough)
				{
					mPassthrough->write('\n');
				}
				/*
				*	If the command response is being recorded THEN
				*	the newline marks the end of the response
				*/
				if (mRxBufferPtr > mRxBuffer)
				{
					*mRxBufferPtr = 0;	// Mark the end of the response string
					mRxBufferPtr = mRxBuffer;	// Rewind the response buffer
					HandleCommandResponse();
				}
				break;
			default:
				if (mPassthrough)
				{
					//while (mPassthrough->availableForWrite() < 3){}
					mPassthrough->write(byteRead);
				}
				{
					uint8_t	rxLen = mRxBufferPtr - mRxBuffer;
					if (rxLen < kRxBufferSize)
					{
						rxLen++;
						*(mRxBufferPtr++) = byteRead;
						*mRxBufferPtr = 0;	// Always terminate for debugging
						if (mSMSStatus == eSMSSending &&
							byteRead == '>')
						{
							SendSMSMessage();
						}
					} else
					{
						// Error, buffer overrun
						// This is a punt, because something is very wrong.
						// The rx buffer is already terminated in begin()
						mRxBufferPtr = mRxBuffer;	  // Rewind
						if (mPassthrough)
						{
							mPassthrough->print(F("\n>>> Buffer overrun\n"));
						}
						HandleCommandResponse();
					}
				}
				break;
		}
	}
	if (mCommandTimeout.Passed())
	{
		HandleCommandTimeout();
	}
	if (mCheckLevelsPeriod.Passed())
	{
		CheckLevels();
	}
}

/*************************** HandleCommandResponse ****************************/
/*
*	This is called when a complete line is received, or a measured string, or
*	a Rx buffer overflow.  If on entry there is an active, unterminated command
*	in progress (a multi line command), then finish processing that command.
*/
void SIM7000::HandleCommandResponse(void)
{
	PauseRx();
	mCommandTimeout.Start();
	bool	handled = false;
	if (mCommandHash)
	{
		handled = HandleMultiLineCommand();
	}
	if (!handled)
	{
		/*
		*	The first character of the response determines the response action.
		*/
		switch(mRxBufferPtr[0])
		{
			case '*':
			{
				char*	rxBufferPtr = &mRxBufferPtr[1];
				/*
				*	If this is "Refresh time and time zone by network" THEN
				*	update the time.
				*/
				if (CmpBufferP(PSTR("PSU"), rxBufferPtr))
				{
					/*
					*	e.g. *PSUTTZ: 21/08/03,19:33:27","-16",1
					*	Note that you only get these unsolicited responses when
					*	the CLTS value is 1. When CLTS=0, the local time isn't
					*	updated.  This means even if you only want to use the
					*	explicite CCLK command, you still have to enable the
					*	SIM7000 RTC with AT+CLTS=1. It appears that you only
					*	need to issue AT+CLTS=1 once and the value will be
					*	stored in the SIM7000 EEPROM.
					*
					*	The unsolicited response is enabled at startup in
					*	HandleCommandCompleted during the startup chain of
					*	commands.
					*/
					UpdateTime(UnixTime::StringToUnixTime(&mRxBufferPtr[9], true));
				} // Else it's probably Network Name (PSNWID), ignore.
				break;
			}
			case '+':
				ParseCommandResponse();
				break;
			case 'A':
				/*
				*	If the response begins with AT or At THEN
				*	Echo is probably on or is in the process of being turned off.
				*	Ignore this line.
				*/
				if (mRxBufferPtr[1] == 'T' ||
					mRxBufferPtr[1] == 't')
				{

				} else
				{
					ParseOtherCommandResponse();
				}
				break;
			case 'E':
				if (CmpBufferP(PSTR("ERROR"), mRxBufferPtr))
				{
					HandleCommandFailed();
				} else
				{
					ParseOtherCommandResponse();
				}
				break;
			case 'D':
				// Update DST, unsolicited, ignore.  e.g. DST: 1
				break;
			case 'N':
				if (CmpBufferP(PSTR("NORMAL POWER DOWN"), mRxBufferPtr))
				{
					mSleepState = eSleeping;
				} else
				{
					ParseOtherCommandResponse();
				}
				break;
			case 'O':
				/*
				*	If the response is OK THEN
				*	the command completed successfully.
				*/
				if (mRxBufferPtr[1] == 'K')
				{
					HandleCommandCompleted();
				} // Else it could be OVER-VOLTAGE POWER DOWN/WARNING
				break;
			case 'R':	// If RDY
				if (mRxBufferPtr[1] != 'D' ||
					mRxBufferPtr[2] != 'Y')
				{
					// RECV FROM, REMOTE IP:, 
					ParseOtherCommandResponse();
				}
				break;
			case 'S':
				if (CmpBufferP(PSTR("SMS Ready"), mRxBufferPtr))
				{
					HandleCommandCompleted();
				}
				break;
			default:
				ParseOtherCommandResponse();
				break;
		}
	}
	FlushRxBuffer();
	ResumeRx();
}


/*************************** HandleMultiLineCommand ***************************/
bool SIM7000::HandleMultiLineCommand(void)
{
	bool	handled = false;
	switch(mCommandHash)
	{
		case kCMGLCmdHash:
		case kCMGRCmdHash:
		{
			TPAddress smscAddr;
			TPAddress sender;
			char message[512];
			uint8_t	messageLen;
			handled = true;
			mCommandHash = 0;
			mRxBufferPtr += ExtractAddress(mRxBufferPtr, true, smscAddr, nullptr);
			messageLen = ParseTPDU(mRxBufferPtr, message, sender);
			MessageRead(message, messageLen, sender, smscAddr);
			break;
		}
	}
	return(handled);
}

/*************************** ProcessQueuedSMSReply ****************************/
// Implemented in subclass
void SIM7000::ProcessQueuedSMSReply(void)
{
}

/**************************** ParseCommandResponse ****************************/
/*
*	Responses of the form +cccc: [<val>,,,] where cccc is the command.
*	The command is converted into a hash value for easier processing.
*/
void SIM7000::ParseCommandResponse(void)
{
	const char*	rxBufferPtr = mRxBufferPtr;
	// The '+' has already been verified by the caller.
	// Scan for the colon dlimiter.
	char		thisChar = *(++rxBufferPtr);
	uint16_t	hash = 0;
	for (; thisChar; thisChar = *(++rxBufferPtr))
	{
		if (thisChar != ':')
		{
			hash = ((hash + thisChar) * thisChar) % 0x1FFF;
			continue;
		}
		mCommandHash = hash;
		rxBufferPtr++;	// skip the colon dlimiter
		thisChar = SkipWhitespaceOnLine(rxBufferPtr);
		if (thisChar &&
			thisChar != '\n')
		{
			switch (hash)
			{
				case kCSQCmdHash:	// CSQ (RSSI)
				{
					uint16_t	rssi;
					if (GetUInt16Value(rxBufferPtr, rssi) == ',')
					{
						/*
						*	The resulting mBars is in the range of 0 to 50,
						*	where 50 is 5 bars.  Ex: 34 would be 3.4 bars.
						*/
						if (rssi == 99 ||
							rssi < 2) 			// -111 dBm or higher
						{
							mBars = 0;							// None
						} else if (rssi < 10)	// -95 to -109 dBm
						{
							mBars = 10 + (((rssi - 2)*10)/8);	// Marginal
						} else if (rssi < 15)	// -85 to -93 dBm
						{
							mBars = 20 + (((rssi - 10)*10)/5);	// OK
						} else if (rssi < 20)	// -75 to -83 dBm
						{
							mBars = 30 + (((rssi - 15)*10)/5);	// Good
						} else					// -51 to -73 dBm
						{
							mBars = 40 + (((rssi - 20)*10)/11);	// Excellent
						}
					} else
					{
						mBars = 99;
					}
					break;
				}
			#if 0
				case kCFUNCmdHash:	// CFUN (Phone Functionality)
				{
					uint16_t	func;
					/*
					*	On wake up, the SIM7000 doesn't always reply with RDY,
					*	but always responds with CFUN.
					*/
					if (mSleepState == eWakingUp &&
						GetUInt16Value(rxBufferPtr, func) == 0 &&
						func == 1)	// Expect full functionality
					{
						mCommandState = eReady;
						TurnOffEchoMode();
					}
					break;
				}
			#endif
				case kCCLKCmdHash:	// CCLK (Clock/Get Local Time)
				{
					if (thisChar == '\"')
					{
						UpdateTime(UnixTime::StringToUnixTime(&rxBufferPtr[1], false));
					}
					break;
				}
				case kCBCCmdHash:
				{
					// Only the level is saved as a percentage.
					//Ex:  0,95,4246 = not charging, 95%, 4.246 volts.
					uint16_t	isCharging, batteryLevel;
					if (GetUInt16Value(rxBufferPtr, isCharging) == ',')
					{
						rxBufferPtr++;	// Skip the comma
						GetUInt16Value(rxBufferPtr, batteryLevel);
						if (batteryLevel <= 100)
						{
							mBatteryLevel = batteryLevel;
						} else
						{
							mBatteryLevel = 0;
						}
					}
					break;
				}
				/*
				*	The connection registration is returned unsolicited. This
				*	unsolicited response is enabled at startup in
				*	HandleCommandCompleted during the startup chain of commands.
				*
				*	There are two response formats for +CREG:
				*	The unsolicited format is "+CREG: 1" where 1 is the
				*	connection status.
				*	The solicited format is "+CREG: 1,1" where the first param
				*	is the unsolicited response state (1 = enable, 0 = disable),
				*	and the 2nd param is the connection status. To differentiate
				*	between the two the response is checked for the delimiter.
				*/
				case kCREGCmdHash:
				{
					mConnectionStatus = rxBufferPtr[rxBufferPtr[1] == ',' ? 2:0] - '0';
					break;
				}
				/*
				*	+CMS ERROR: <error> marks the end of a command that failed.
				*/
				case kCMS_ERRORCmdHash:
				{
					mCommandState = eError;
					mCommandTimeout.Set(0);
					if (mSMSStatus == eSMSWaiting)
					{
						mSMSStatus = eSMSFailed;
					}
					break;
				}
				/*
				*	+CME ERROR: <error> marks the end of a command that failed.
				*	At some point it may be useful to interpret the command by
				*	setting AT+CMEE=1 to get a numeric response.  For now this
				*	is only returned when the user sends AT+CMEE=2 to get a
				*	verbose response.  The default, AT+CMEE=0, responds with
				*	simply ERROR, not +CME ERROR:<error>.
				*/
				case kCME_ERRORCmdHash:
				{
					mCommandState = eError;
					mCommandTimeout.Set(0);
					break;
				}

				case kCMTICmdHash:	// CMTI is an unsolicited result code.
				{					// It means a new message has been received.
					// Example
					// +CMTI: "SM",3 ==> stored in SIM, index 3
					// Use AT+CMGR=3 to read. CMGR format depends on CMGF, where
					// 1 = text mode, 0 = PDU mode.
					// Store the returned index in a ring buffer.
					if (SkipTillChar(',', true, rxBufferPtr))
					{
						uint16_t	messageIndex;
						GetUInt16Value(rxBufferPtr, messageIndex);
						mPendingMessages[mPendingMessagesTail] = messageIndex;
						mPendingMessagesTail++;
						if (mPendingMessagesTail == sizeof(mPendingMessages))
						{
							mPendingMessagesTail = 0;
						}
					}
					break;
				}
				case kCMGLCmdHash:	// List is handled the same as read.
				// +CMGL: 5,1,,22 CMGL lists the index as the first param, but
				// otherwise is the same as CMGR.
				case kCMGRCmdHash:
				{
				// Text Mode:
				// +CMGR: "REC READ","+15118333317",,"21/08/17,18:22:43-16"
				// ack
				// PDU Mode
				// +CMGR: 1,,22	1= read,,22 = length of TPDU octets 44 bytes
				// 07919130364886F2040B915080173313F700001280718122346903E1F11A
					/*
					*	If the response is for text mode...
					*/
					if (thisChar == '\"' ||			// CMGR
						rxBufferPtr[2] == '\"' ||	// CMGL index 0 to 9
						rxBufferPtr[3] == '\"')	// CMGL index 10 to 15
					{
						mCommandHash = 0;	// don't handle it.
					}
					break;
				}
				/*
				*	+CMGS is an unsolicited command response received when an
				*	SMS was successfully sent.  This means that the SMSC has
				*	received the SMS and delivery will be attempted.
				*	Example:   +CMGS: 29
				*/
				case kCMGSCmdHash:
				{
					if (mSMSStatus == eSMSWaiting)
					{
						mSMSStatus = eSMSSent;
					}
					break;
				}
				case kCPINCmdHash:
				{
					char*	dummyBufPtr = (char*)rxBufferPtr;
					if (CmpBufferP(PSTR("NOT INSERTED"), dummyBufPtr))
					{
						HandleNoSIMCardFound();
					}
					break;
				}
			}
		}
		break;
	}
	mRxBufferPtr = (char*)rxBufferPtr;

}

/********************************* UpdateTime *********************************/
void SIM7000::UpdateTime(
	uint32_t	inTime)
{
	if (inTime)
	{
		time32_t timeDelta;
		if (UnixTime::Time() < inTime)
		{
			timeDelta = inTime - UnixTime::Time();
		} else
		{
			timeDelta = UnixTime::Time() - inTime;
		}
		UnixTime::SetTime(inTime);
		// To detect a huge change in time change, such as
		// after a cold start or timezone change...
		if (timeDelta > (UnixTime::SleepDelay()+10))
		{
			UnixTime::ResetSleepTime();
		}
		mTimeIsValid = true;
	}
}

/******************************** MessageRead *********************************/
void SIM7000::MessageRead(
	const char*			inMessage,
	uint8_t				inMessageLen,
	const TPAddress&	inSender,
	const TPAddress&	inSMSCAddr)
{
	if (mPassthrough)
	{
		mPassthrough->write('\"');
		mPassthrough->print(inMessage);
		mPassthrough->print(F("\", "));
		mPassthrough->print(inMessageLen);
		mPassthrough->print(F(", "));
		mPassthrough->print(inSender);
		mPassthrough->print(F(", "));
		mPassthrough->print(inSMSCAddr);
		mPassthrough->write('\n');
	}
	if (mWaitingToProcessMessage)
	{
		if (mDeleteMessagesAfterRead)
		{
			mWaitingToDeleteMessage = mWaitingToProcessMessage;
		}
		mWaitingToProcessMessage = 0;
	}
}

/*************************** HandleCommandCompleted ***************************/
/*
*	Called when the active command successfully completed.  This generally means
*	OK was returned by the SIM7000.  ParseCommandResponse may have been called
*	before it gets here, if the command had any '+' reponse values.
*/
void SIM7000::HandleCommandCompleted(void)
{
	mCommandTimeout.Set(0);	// Disable timeout timer (Passed will return false)
	uint16_t	commandHash = mCommandHash;
	mCommandHash = 0;
	mCommandState = eReady;
	if (mSleepState != eWakingUp)
	{
		switch (commandHash)
		{
			case kATE0CmdHash:
			case kIPRCmdHash:
				/*
				*	If it took more than one attempt to get here, THEN
				*	the baud rate needs to be set.
				*/
				if (mRetries != 0 &&
					mRetries < kAutobaudEchoRetries)
				{
					SendCommand(F("AT+IPR=9600"), kIPRCmdHash);
				} else
				{
					// Enable XOFF/XON flow control for Rx only
					// Enable unsolicited time updates and the SIM7000 RTC.
					// Enable unsolicited connection registration changes.
					SendCommand(F("AT+IFC=1;+CLTS=1;+CREG=1"), kIFCCmdHash);
				}
				break;
			case kIFCCmdHash:
				CheckLevels();
				break;
		}
		ProcessQueuedSMSReply();	// If any
	} else	// else it's waking up...
	{
		mSleepState = eRunning;
		TurnOffEchoMode();
	}
}

/**************************** HandleCommandTimeout ****************************/
void SIM7000::HandleCommandTimeout(void)
{
	if (mPassthrough)
	{
		mPassthrough->print(F("timeout\n"));
	}
	uint16_t	commandHash = mCommandHash;
	mCommandHash = 0;
	mCommandState = eTimeout;
	mCommandTimeout.Set(0);	// Disable timeout timer (Passed will return false)
	if (mSleepState != eWakingUp)
	{
		switch (commandHash)
		{
			case kATE0CmdHash:
				if (mRetries)
				{
					mRetries--;
					TurnOffEchoMode(mRetries);
					break;
				} // else fall through to default as a timeout.
			default:
				break;
		}
	/*
	*	else the SIM7000 is waking up...
	*
	*	If the the SIM7000 is awake THEN
	*	autobaud may be set, or a reset was made.  To establish the baud rate,
	*	keep sending "ATE0" (turn off echo) till OK is the response. When the
	*	SIM7000 baud rate is set to 0 (autobaud) powerup will not respond with
	*	anything, it will just startup and timeout.
	*/
	} else if (digitalRead(mRxPin))
	{
		mSleepState = eRunning;
		TurnOffEchoMode(kAutobaudEchoRetries);
	} else
	{
		// Else this is a major failure.  The SIM7000 hasn't started.
		mCommandState = eError;
		if (mPassthrough)
		{
			mPassthrough->print(F("Wakeup/Reset Failed.\n"));
		}
	}
}

/**************************** HandleCommandFailed *****************************/
void SIM7000::HandleCommandFailed(void)
{
	//uint16_t	commandHash = mCommandHash;
	mCommandHash = 0;
	mCommandState = eError;
	//if (mSleepState != eWakingUp)
	//{
	//	switch (commandHash)
	//	{
	//	}
	//}
}

/************************* ParseOtherCommandResponse **************************/
void SIM7000::ParseOtherCommandResponse(void)
{
}

/**************************** SetCheckLevelsPeriod ****************************/
/*
*	inPeriod in milliseconds.
*	Pass 0 to disable automatic levels check.
*/
void SIM7000::SetCheckLevelsPeriod(
	uint32_t	inPeriod)
{
	mCheckLevelsPeriod.Set(inPeriod);
}

/******************************** CheckLevels *********************************/
/*
*	Initiates a check levels set of commands.
*	- Check the RSSI (signal strength)
*	- Check the battery level
*/
void SIM7000::CheckLevels(void)
{
	if (mSMSStatus != eSMSWaiting)
	{
		SendCommand(F("AT+CSQ;+CBC"), 0, 2000);
	}
	if (mCheckLevelsPeriod.Get())
	{
		mCheckLevelsPeriod.Start();
	}
}

/****************************** TurnOffEchoMode *******************************/
void SIM7000::TurnOffEchoMode(
	uint8_t	inRetries)
{
	mRetries = inRetries;
	SendCommand(F("ATE0"), kATE0CmdHash);
}

//const uint8_t	kPDUSubmitPreambleSize = 28;	// If not sending SMSC params
const uint8_t	kPDUSubmitPreambleSize = 30;	// If sending SMSC params
/********************************** SendSMS ***********************************/
bool SIM7000::SendSMS(
	const char*	inPhoneNumber,
	const char*	inMessage)
{
#ifdef USE_PDU_SMS_FORMAT
	bool sent = ClearToSendSMS() &&
		strlen(inMessage) < ((kTxBufferSize-kPDUSubmitPreambleSize)*8)/7;
	if (sent)
	{
		FlushRxBuffer();
		mSerial.print(F("AT+CMGF=0;+CMGS="));
		mSerial.print(CreateSMSSubmitPDU(inPhoneNumber, inMessage, true, mTxBuffer), DEC);
		mSerial.println();
		
		mSMSStatus = eSMSSending;
	}
#else
	bool sent = ClearToSendSMS() && strlen(inMessage) < kTxBufferSize;
	if (sent)
	{
		FlushRxBuffer();
		mSerial.print(F("AT+CMGF=1;+CMGS=\""));
		mSerial.print(inPhoneNumber);
		mSerial.println('\"');
		
		strcpy(mTxBuffer, inMessage);
		mSMSStatus = eSMSSending;
	}
#endif
	return(sent);
}

/******************************* SendSMSMessage *******************************/
/*
*	This is called from Update when "> " is received from the SIM7000 when the
*	mSMSStatus is eSMSSending.  After sending the message the mSMSStatus is set
*	to eSMSWaiting.  The SMS will either send or fail.  If it sends, the SIM7000
*	will respond with +CMGS <index>, where index is the index of the SMS
*	accepted by the SMSC.
*/
void SIM7000::SendSMSMessage(void)
{
	if (mSMSStatus == eSMSSending)
	{
		mSMSStatus = eSMSWaiting;
		FlushRxBuffer();
		mSerial.print(mTxBuffer);
		mSerial.print('\x1A');
	}
}

/******************************** SendCommand *********************************/
bool SIM7000::SendCommand(
	const __FlashStringHelper*	inCommandStr,
	uint16_t					inCommandHash,
	uint16_t					inCommandTimeout)
{
	char	commandStr[50];
	strcpy_P(commandStr, (const char*)inCommandStr);
	return(SendCommand(commandStr, inCommandHash, inCommandTimeout));
}

/******************************** SendCommand *********************************/
/*
*	Pass inCommandHash = 0 for any command that has a + response otherwise the
*	response will be passed to HandleMultiLineCommand rather than
*	ParseCommandResponse and will fail.  (See the top of HandleCommandResponse)
*	inCommandHash is used by commands that generate an OK/ERROR response to
*	know when the specified command has completed.
*/
bool SIM7000::SendCommand(
	const char*	inCommandStr,
	uint16_t	inCommandHash,
	uint16_t	inCommandTimeout)
{
	bool success = ClearToSend();
	if (success)
	{
		mCommandHash = inCommandHash;
		FlushRxBuffer();
		mSerial.println(inCommandStr);
		mCommandState = eBusy;
		mCommandTimeout.Set(inCommandTimeout);
		mCommandTimeout.Start();
		if (mPassthrough)
		{
			mPassthrough->print('>');
			mPassthrough->print(inCommandStr);
			mPassthrough->print('\n');
		}
	} else if (mPassthrough)
	{
		mPassthrough->print(F("busy\n"));
	}
	return(success);
}

/******************************** DumpRxBuffer ********************************/
#if 0
void SIM7000::DumpRxBuffer(void) const
{
	if (mPassthrough)
	{
		mPassthrough->write('\n');
		mPassthrough->write(1);	// Force dump
		mPassthrough->print(mRxBuffer);
		mPassthrough->write('\n');
	}
}
#endif
/********************************* ClearError *********************************/
void SIM7000::ClearError(void)
{
	mCommandState = eReady;
	FlushRxBuffer();
}

/****************************** GetSleepStateStr ******************************/
const __FlashStringHelper * SIM7000::GetSleepStateStr(
	uint8_t	inState)
{
	return((const __FlashStringHelper*)pgm_read_word(&kSleepStateNames[inState]));
}

/***************************** GetCommandStateStr *****************************/
const __FlashStringHelper * SIM7000::GetCommandStateStr(
	uint8_t	inState)
{
	return((const __FlashStringHelper*)pgm_read_word(&kCommandStateNames[inState]));
}
