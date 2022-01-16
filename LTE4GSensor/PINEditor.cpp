/*
*	PINEditor.cpp, Copyright Jonathan Mackey 2021
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
#include "PINEditor.h"
#include "DisplayController.h"
#include "XFont.h"
#include <avr/pgmspace.h>

const char kSetStr[] PROGMEM = "Set";
const char kCancelStr[] PROGMEM = "Cancel";


#define FONT_HEIGHT	43
#define DISPLAY_WIDTH	240
const Rect8_t PINEditor::kField[6] PROGMEM =
{
	{60,	FONT_HEIGHT, 34, FONT_HEIGHT},	// eDigit0Field
	{94,	FONT_HEIGHT, 34, FONT_HEIGHT},	// eDigit1Field
	{128,	FONT_HEIGHT, 34, FONT_HEIGHT},	// eDigit2Field
	{162,	FONT_HEIGHT, 34, FONT_HEIGHT},	// eDigit3Field
	{25,	FONT_HEIGHT*2, 56, FONT_HEIGHT},// eSet
	{106,	FONT_HEIGHT*2, 109, FONT_HEIGHT}// eCancel
};
/*		
		.0..0..0..0.		136px
		.0.	 				34px
		   .0.				34px
			  .0.			34px
				 .0.		34px
		.SET..CANCEL.		25 56 25 109 25 = 240
		
*/

/******************************* PINEditor *******************************/
PINEditor::PINEditor(void)
{
}

/********************************* Initialize *********************************/
void PINEditor::Initialize(
	XFont*	inXFont)
{
	mXFont = inXFont;
	mDisplay = inXFont->GetDisplay();
	mDirtyField = 0;
}

/*************************** LeftRightButtonPressed ***************************/
/*
*	Moves to the next or previous field.
*/
void PINEditor::LeftRightButtonPressed(
	bool	inIncrement)
{
	mDisplay->DrawFrame8(&mSelectionRect, XFont::eBlack, 2);
	if (inIncrement)
	{
		mSelection++;
		if (mSelection >= eNumFields)
		{
			mSelection = 0;
		}
	} else if (mSelection > 0)
	{
		mSelection--;
	} else
	{
		mSelection = eNumFields-1;
	}
	
	GetAdjustedFieldRect(mSelection, mSelectionRect);
}

/**************************** UpDownButtonPressed *****************************/
void PINEditor::UpDownButtonPressed(
	bool	inIncrement)
{
	if (mSelection < eNumPINFields)
	{
		uint8_t&	pinValue = mPIN[mSelection];
		if (inIncrement)
		{
			pinValue++;
			if (pinValue > 9)
			{
				pinValue = 0;
			}
		} else if (pinValue > 0)
		{
			pinValue--;
		} else
		{
			pinValue = 9;
		}
		mDirtyField |= _BV(mSelection);
		DrawPIN();
	}
}

/******************************** EnterPressed ********************************/
bool PINEditor::EnterPressed(void)
{
	bool done = mSelection >= eNumPINFields;
	if (!done)
	{
		LeftRightButtonPressed(true);
	}
	return(done);
}


/**************************** GetAdjustedFieldRect ****************************/
void PINEditor::GetAdjustedFieldRect(
	uint8_t		inFieldIndex,
	Rect8_t&	outRect)
{
	memcpy_P(&outRect, &kField[inFieldIndex], 4);
}

/********************************** DrawPIN ***********************************/
void PINEditor::DrawPIN(void)
{
	char pinStr[eNumPINFields+1];
	
	for (uint8_t field = 0; field < eNumPINFields; field++)
	{
		pinStr[field] = mPIN[field] + '0';
	}
	pinStr[eNumPINFields] = 0;

	mDisplay->MoveToRow(0);
	mXFont->SetTextColor(XFont::eCyan);
	mXFont->DrawCentered(pinStr);
}

/****************************** DrawPINFields ******************************/
void PINEditor::DrawPINFields(void)
{
	Rect8_t	fieldRect;

	for (uint8_t field = 0; field < eNumPINFields; field++)
	{
		GetAdjustedFieldRect(field, fieldRect);
		DrawField(field, fieldRect);
	}
}

/*********************************** SetPIN ***********************************/
void PINEditor::SetPIN(
	uint16_t	inPIN)
{
	Rect8_t	fieldRect;
	mDisplay->Fill();	// Erase the display

	for (uint8_t i = eNumPINFields; i;)
	{
		i--;
		mPIN[i] = inPIN%10;
		inPIN /= 10;
	}

	mSelection = 0;
	mSelectionIndex = 0;
	GetAdjustedFieldRect(0, mSelectionRect);
	mSelectionPeriod.Set(500);
	mSelectionPeriod.Start();
	
	DrawPIN();
	DrawPINFields();
	GetAdjustedFieldRect(eSet, fieldRect);
	DrawField(eSet, fieldRect);
	GetAdjustedFieldRect(eCancel, fieldRect);
	DrawField(eCancel, fieldRect);
}

/*********************************** GetPIN ***********************************/
uint16_t PINEditor::GetPIN(void) const
{
	uint16_t	pin = 0;
	for (uint8_t i = 0; i < 4; i++)
	{
		pin = (pin*10) + mPIN[i];
	}
	return(pin);
}

/********************************* DrawField **********************************/
void PINEditor::DrawField(
	uint8_t		inField,
	Rect8_t&	inFieldRect)
{
	char fieldStr[10];
	mDisplay->MoveToRow(inFieldRect.y+5);

	if (inField < eNumPINFields)
	{
		fieldStr[0] = mPIN[inField] + '0';
		fieldStr[1] = 0;
		mXFont->SetTextColor(XFont::eMagenta);
	} else if (inField == eSet)
	{
			mXFont->SetTextColor(XFont::eGreen);
			strcpy_P(fieldStr, kSetStr);
	} else	// eCancel
	{
		mXFont->SetTextColor(XFont::eRed);
		strcpy_P(fieldStr, kCancelStr);
	}
	mXFont->DrawCentered(fieldStr, inFieldRect.x, inFieldRect.x+inFieldRect.width);
	
}

/*********************************** Update ***********************************/
void PINEditor::Update(void)
{
	if (mDirtyField)
	{
		uint8_t	field = 0;
		Rect8_t	fieldRect;
		for (uint8_t mask = 1; mask; mask <<= 1, field++)
		{
			if ((mDirtyField & mask) == 0)
			{
				continue;
			}
			GetAdjustedFieldRect(field, fieldRect);
			DrawField(field, fieldRect);
		}
		mDirtyField = 0;
	}
	if (mSelectionPeriod.Passed())
	{
		mSelectionIndex++;
		uint16_t	selectionColor = (mSelectionIndex & 1) ? XFont::eWhite : XFont::eBlack;
		mDisplay->DrawFrame8(&mSelectionRect, selectionColor, 2);
		mSelectionPeriod.Start();
	}
}
