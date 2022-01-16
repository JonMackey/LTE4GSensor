/*
*	SIM7000ATCmdHash.h
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
*
*	The hash values below were generated using a command line tool that applied
*	the hash equation to each AT+ command suffix character.
*
*		hash = ((hash + cmdChar) * cmdChar) % 0x1FFF;
*
*		Note depending on the compiler defaults, casting to uint16_t may
*		be needed in order to get the same values as generated.
*
*		((uint16_t)((uint16_t)(hash + cmdChar) * cmdChar)) % 0x1FFF;
*
*	Example:
*		const uint16_t kCFUNCmdHash			= 5504;	// AT+CFUN
*		// CFUN = C F U N = 67 70 85 78
*		uint16_t	hash = 0;
*		hash = ((0 + 67) * 67) % 0x1FFF		= 4489
*		hash = ((4489 + 70) * 70) % 0x1FFF	= 7840
*		hash = ((7840 + 85) * 85) % 0x1FFF	= 1883
*		hash = ((1883 + 78) * 78) % 0x1FFF	= 5504
*
*	Note that the hash equation is not universal.  The values have only been
*	verified unique for the set of AT commands listed.  The purpose of the hash
*	is to avoid a binary search of the command string.
*/
#ifndef SIM7000ATCmdHash_H
#define SIM7000ATCmdHash_H

const uint16_t kATE0CmdHash			= 4135;	// ATE0
const uint16_t kCADCCmdHash			= 2084;	// +CADC
const uint16_t kCBANDCmdHash		= 7975;	// +CBAND
const uint16_t kCBATCHKCmdHash		= 1038;	// +CBATCHK
const uint16_t kCBCCmdHash			= 2846;	// +CBC
const uint16_t kCCIDCmdHash			= 3685;	// +CCID
const uint16_t kCCLKCmdHash			= 5436;	// +CCLK
const uint16_t kCDEVICECmdHash		= 3423;	// +CDEVICE
const uint16_t kCDNSCFGCmdHash		= 3183;	// +CDNSCFG
const uint16_t kCDNSGIPCmdHash		= 3187;	// +CDNSGIP
const uint16_t kCEDRXCmdHash		= 5950;	// +CEDRX
const uint16_t kCFGRICmdHash		= 834;	// +CFGRI
const uint16_t kCFUNCmdHash			= 5504;	// +CFUN
const uint16_t kCGACTCmdHash		= 7916;	// +CGACT
const uint16_t kCGATTCmdHash		= 4471;	// +CGATT
const uint16_t kCGDCONTCmdHash		= 1998;	// +CGDCONT
const uint16_t kCGMICmdHash			= 4522;	// +CGMI
const uint16_t kCGMMCmdHash			= 3278;	// +CGMM
const uint16_t kCGMRCmdHash			= 1778;	// +CGMR
const uint16_t kCGPADDRCmdHash		= 4309;	// +CGPADDR
const uint16_t kCGPIOCmdHash		= 2529;	// +CGPIO
const uint16_t kCGREGCmdHash		= 6103;	// +CGREG
const uint16_t kCGSMSCmdHash		= 6440;	// +CGSMS
const uint16_t kCGSNCmdHash			= 8167;	// +CGSN
const uint16_t kCIFSRCmdHash		= 5393;	// +CIFSR
const uint16_t kCIFSREXCmdHash		= 4129;	// +CIFSREX
const uint16_t kCIICRCmdHash		= 7427;	// +CIICR
const uint16_t kCIMICmdHash			= 5733;	// +CIMI
const uint16_t kCIPACKCmdHash		= 2527;	// +CIPACK
const uint16_t kCIPATSCmdHash		= 5867;	// +CIPATS
const uint16_t kCIPCCFGCmdHash		= 1332;	// +CIPCCFG
const uint16_t kCIPCLOSECmdHash		= 4441;	// +CIPCLOSE
const uint16_t kCIPCSGPCmdHash		= 6788;	// +CIPCSGP
const uint16_t kCIPDPDPCmdHash		= 2803;	// +CIPDPDP
const uint16_t kCIPHEADCmdHash		= 6735;	// +CIPHEAD
const uint16_t kCIPHEXSCmdHash		= 2231;	// +CIPHEXS
const uint16_t kCIPMODECmdHash		= 3925;	// +CIPMODE
const uint16_t kCIPMUXCmdHash		= 4971;	// +CIPMUX
const uint16_t kCIPQSENDCmdHash		= 7247;	// +CIPQSEND
const uint16_t kCIPRDTIMERCmdHash	= 6050;	// +CIPRDTIMER
const uint16_t kCIPRXGETCmdHash		= 45;	// +CIPRXGET
const uint16_t kCIPSENDCmdHash		= 3535;	// +CIPSEND
const uint16_t kCIPSENDHEXCmdHash	= 6008;	// +CIPSENDHEX
const uint16_t kCIPSERVERCmdHash	= 1321;	// +CIPSERVER
const uint16_t kCIPSGTXTCmdHash		= 6224;	// +CIPSGTXT
const uint16_t kCIPSHOWTPCmdHash	= 6256;	// +CIPSHOWTP
const uint16_t kCIPSHUTCmdHash		= 2543;	// +CIPSHUT
const uint16_t kCIPSPRTCmdHash		= 243;	// +CIPSPRT
const uint16_t kCIPSRIPCmdHash		= 8017;	// +CIPSRIP
const uint16_t kCIPSTARTCmdHash		= 5321;	// +CIPSTART
const uint16_t kCIPSTATUSCmdHash	= 146;	// +CIPSTATUS
const uint16_t kCIPUDPMODECmdHash	= 5520;	// +CIPUDPMODE
const uint16_t kCLCKCmdHash			= 1313;	// +CLCK
const uint16_t kCLPORTCmdHash		= 7825;	// +CLPORT
const uint16_t kCLTSCmdHash			= 6426;	// +CLTS
const uint16_t kCME_ERRORCmdHash	= 4920;	// +CME ERROR
const uint16_t kCMEECmdHash			= 1561;	// +CMEE
const uint16_t kCMGDCmdHash			= 3038;	// +CMGD
const uint16_t kCMGFCmdHash			= 8087;	// +CMGF
const uint16_t kCMGLCmdHash			= 6892;	// +CMGL
const uint16_t kCMGRCmdHash			= 5769;	// +CMGR
const uint16_t kCMGSCmdHash			= 4225;	// +CMGS
const uint16_t kCMGWCmdHash			= 6260;	// +CMGW
const uint16_t kCMNBCmdHash			= 2067;	// +CMNB
const uint16_t kCMS_ERRORCmdHash	= 4701;	// +CMS ERROR
const uint16_t kCMSSCmdHash			= 4726;	// +CMSS
const uint16_t kCMTICmdHash			= 4565;	// +CMTI
const uint16_t kCNBPCmdHash			= 356;	// +CNBP
const uint16_t kCNETLIGHTCmdHash	= 536;	// +CNETLIGHT
const uint16_t kCNMICmdHash			= 5254;	// +CNMI
const uint16_t kCNMPCmdHash			= 2054;	// +CNMP
const uint16_t kCNSMODCmdHash		= 7913;	// +CNSMOD
const uint16_t kCNVRCmdHash			= 8110;	// +CNVR
const uint16_t kCNVWCmdHash			= 1251;	// +CNVW
const uint16_t kCOPNCmdHash			= 6015;	// +COPN
const uint16_t kCOPSCmdHash			= 5237;	// +COPS
const uint16_t kCPASCmdHash			= 4474;	// +CPAS
const uint16_t kCPINCmdHash			= 5717;	// +CPIN
const uint16_t kCPMSCmdHash			= 7876;	// +CPMS
const uint16_t kCPOLCmdHash			= 7102;	// +CPOL
const uint16_t kCPOWDCmdHash		= 3771;	// +CPOWD
const uint16_t kCPSMSCmdHash		= 4585;	// +CPSMS
const uint16_t kCPWDCmdHash			= 1373;	// +CPWD
const uint16_t kCRCCmdHash			= 1229;	// +CRC
const uint16_t kCREGCmdHash			= 7198;	// +CREG
const uint16_t kCRESCmdHash			= 3527;	// +CRES
const uint16_t kCRSMCmdHash			= 2775;	// +CRSM
const uint16_t kCSASCmdHash			= 7438;	// +CSAS
const uint16_t kCSCACmdHash			= 7908;	// +CSCA
const uint16_t kCSCLKCmdHash		= 5165;	// +CSCLK
const uint16_t kCSCSCmdHash			= 2387;	// +CSCS
const uint16_t kCSDHCmdHash			= 945;	// +CSDH
const uint16_t kCSGSCmdHash			= 2388;	// +CSGS
const uint16_t kCSIMCmdHash			= 1069;	// +CSIM
const uint16_t kCSMPCmdHash			= 3011;	// +CSMP
const uint16_t kCSMSCmdHash			= 2658;	// +CSMS
const uint16_t kCSQCmdHash			= 30;	// +CSQ
const uint16_t kCSTTCmdHash			= 6339;	// +CSTT
const uint16_t kCUSDCmdHash			= 2225;	// +CUSD
const uint16_t kGCAPCmdHash			= 2898;	// +GCAP
const uint16_t kGMICmdHash			= 3303;	// +GMI
const uint16_t kGMMCmdHash			= 7383;	// +GMM
const uint16_t kGMRCmdHash			= 4337;	// +GMR
const uint16_t kGOICmdHash			= 286;	// +GOI
const uint16_t kGSNCmdHash			= 1494;	// +GSN
const uint16_t kGSVCmdHash			= 5493;	// +GSV
const uint16_t kHTTPACTIONCmdHash	= 6704;	// +HTTPACTION
const uint16_t kHTTPDATACmdHash		= 1570;	// +HTTPDATA
const uint16_t kHTTPHEADCmdHash		= 3434;	// +HTTPHEAD
const uint16_t kHTTPINITCmdHash		= 4499;	// +HTTPINIT
const uint16_t kHTTPPARACmdHash		= 7338;	// +HTTPPARA
const uint16_t kHTTPREADCmdHash		= 5780;	// +HTTPREAD
const uint16_t kHTTPSTATUSCmdHash	= 2153;	// +HTTPSTATUS
const uint16_t kHTTPTERMCmdHash		= 5498;	// +HTTPTERM
const uint16_t kHTTPTOFSCmdHash		= 7109;	// +HTTPTOFS
const uint16_t kICFCmdHash			= 7333;	// +ICF
const uint16_t kIFCCmdHash			= 4730;	// +IFC
const uint16_t kIPRCmdHash			= 2352;	// +IPR
const uint16_t kSGPIOCmdHash		= 6640;	// +SGPIO
const uint16_t kSLEDCmdHash			= 5425;	// +SLED
#endif