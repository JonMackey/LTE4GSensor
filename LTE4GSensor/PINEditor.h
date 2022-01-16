/*
*	PINEditor.h, Copyright Jonathan Mackey 2021
*	Edits and draws a 4 digit PIN.  This is written for a 240 pixel wide
*	display with a font height of 43 pixels.
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
#ifndef PINEditor_h
#define PINEditor_h

#include "MSPeriod.h"
#include "DisplayController.h"

class XFont;

class PINEditor
{
public:
	enum EField
	{
		eDigit0Field,
		eDigit1Field,
		eDigit2Field,
		eDigit3Field,
		eNumPINFields,
		eSet = eNumPINFields,
		eCancel,
		eNumFields
	};
							PINEditor(void);
	void					Initialize(
								XFont*					inXFont);
	void					Update(void);
	void					SetPIN(
								uint16_t				inPIN);
	uint16_t				GetPIN(void) const;
	bool					CancelIsSelected(void) const
								{return(mSelection == eCancel);}
	bool					EnterPressed(void);
	void					UpDownButtonPressed(
								bool					inIncrement);
	void					LeftRightButtonPressed(
								bool					inIncrement);
protected:
	MSPeriod			mSelectionPeriod;
	XFont*				mXFont;
	DisplayController*	mDisplay;
	uint8_t				mPIN[eNumPINFields];
	uint8_t				mDirtyField;
	uint8_t				mSelection;
	uint8_t				mSelectionIndex;
	Rect8_t				mSelectionRect;
	static const Rect8_t kField[];
	
	void					DrawPINFields(void);
	void					DrawPIN(void);
	void					GetAdjustedFieldRect(
								uint8_t					inFieldIndex,
								Rect8_t&				outRect);
	void					DrawField(
								uint8_t					inField,
								Rect8_t&				inFieldRect);
};

#endif // PINEditor_h
