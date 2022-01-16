/*
0  1  2  3  4  5  6  7  8  9
h  e  l  l  o  h  e  l  l  o
68 65 6C 6C 6F 68 65 6C 6C 6F                   	hellohello

				x       xx      xxx     xxxx    xxxxx   xxxxxx  xxxxxxx                 x
.1101000 .1100101 .1101100 .1101100 .1101111 .1101000 .1100101 .1101100 .1101100 .1101111
68		 65		  6C	   6C		6F		 68		  65	   6C		6C		 6F 

x		 xx		  xxx	   xxxx     xxxxx    xxxxxx   xxxxxxx  x
11101000 00110010 10011011 11111101 01000110 10010111 11011001 11101100 ..110111
E8		 32		  9B	   FD		46		 97		  D9	   EC		37

0  1  2  3  4  5  6  7  8
E8 32 9B FD 46 97 D9 EC 37

				x       xx      xxx     xxxx    xxxxx   xxxxxx  xxxxxxx                 x
.1101000 .1100101 .1101100 .1101100 .1101111 .1101000 .1100101 .1101100 .1101100 .1101111
68		 65		  6C	   6C		6F		 68		  65	   6C		6C		 6F 

h septet[0] = octet[0] & 0x7F;
e septet[1] = ((octet[1] << 1) + (octet[0] >> 7)) & 0x7F;
l septet[2] = ((octet[2] << 2) + (octet[1] >> 6)) & 0x7F;
l septet[3] = ((octet[3] << 3) + (octet[2] >> 5)) & 0x7F;
o septet[4] = ((octet[4] << 4) + (octet[3] >> 4)) & 0x7F;
h septet[5] = ((octet[5] << 5) + (octet[4] >> 3)) & 0x7F;
e septet[6] = ((octet[6] << 6) + (octet[5] >> 2)) & 0x7F;
l septet[7] = octet[6] >> 1;
l septet[8] = octet[7] & 0x7F;
o septet[9] = ((octet[8] << 1) + (octet[7] >> 7)) & 0x7F;

*/
#if 0
uint8_t Pack7To8bit(
	char*		in7BitStr,
	uint8_t*	out8BitData)
{
	uint8_t	octet = *(in7BitStr++);
	uint8_t	shift = 0;
	uint8_t septet;
	uint8_t	dataIndex = 0;

	if (octet)
	{
		while (true)
		{
			septet = *(in7BitStr++);
			if (septet)
			{
				out8BitData[dataIndex] = octet + (septet<<(7-shift));
			} else
			{
				out8BitData[dataIndex] = octet;
				break;
			}
			dataIndex++;
			shift++;
			if (shift < 7)
			{
				octet = septet >> shift;
			} else
			{
				shift = 0;
				octet = *(in7BitStr++);;
			}
		}
	}
	return(dataIndex);
}

h septet[0] = octet[0] & 0x7F;
e septet[1] = ((octet[1] << 1) + (octet[0] >> 7)) & 0x7F;
l septet[2] = ((octet[2] << 2) + (octet[1] >> 6)) & 0x7F;
l septet[3] = ((octet[3] << 3) + (octet[2] >> 5)) & 0x7F;
o septet[4] = ((octet[4] << 4) + (octet[3] >> 4)) & 0x7F;
h septet[5] = ((octet[5] << 5) + (octet[4] >> 3)) & 0x7F;
e septet[6] = ((octet[6] << 6) + (octet[5] >> 2)) & 0x7F;
l septet[7] = octet[6] >> 1;
l septet[8] = octet[7] & 0x7F;
o septet[9] = ((octet[8] << 1) + (octet[7] >> 7)) & 0x7F;

void Unpack8To7bit(
	uint8_t		inDataLen,
	uint8_t*	in8BitData,
	char*		out7BitStr)
{
	uint8_t	octet = *(in8BitData++);
	uint8_t	shift = 0;
	uint8_t septet = octet;

	while (inDataLen)
	{
		*(out7BitStr++) = septet & 0x7F;
		inDataLen--;
		if (inDataLen)
		{
			septet = octet >> (7 - shift);
			if (shift < 6)
			{
				shift++;
				octet = *(in8BitData++);
				septet += (octet << shift);
			} else
			{
				*(out7BitStr++) = (octet >> 1) & 0x7F;
				inDataLen--;
				if (inDataLen)
				{
					septet = octet = *(in8BitData++);
					shift = 0;
				} else
				{
					break;
				}
			}
			continue;
		}
		break;
	}
	*out7BitStr = 0;
}
#endif
