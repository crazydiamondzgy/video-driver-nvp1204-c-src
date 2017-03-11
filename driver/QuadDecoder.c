#include "NVP1204.h"

extern DWORD tNVP1204_500[MAX_VIDEO_FORMAT][51];
extern DWORD tNVP1204_600[54];
//========================================================================//
void Chip_Reset(PDEVICE_EXTENSION pdx, DWORD dwReset)
{
	DWORD	dwData;

	WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + MUX_DIR), MUX_DIR_IN);
	dwData = READ_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + MUX_OUT));
	dwData = dwData | dwReset;
	WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + MUX_DIR), MUX_DIR_OUT);
	WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + MUX_OUT), dwData);

	dwData = dwData & (~dwReset);
	WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + MUX_OUT), dwData);	
	KeStallExecutionProcessor(100);
	
	dwData = dwData | dwReset;
	WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + MUX_OUT), dwData);
}
//========================================================================//
void Decoder_Init(PDEVICE_EXTENSION pdx)
{
	int		i, j, k;
	BYTE	*tChannel_1;
	DWORD	dwdecoder_ip, dwData, *tChannel_4;
	
//	Chip_Reset(pdx, RESET_DECODER);
	KdPrint(("Decoder Init \n"));

	tChannel_4 = tNVP1204_500[pdx->m_byVideoFormat];
	for (j=0; j<51; j++)																		// 0x520 ~ 0x5EF
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x520 + j*4), tChannel_4[j]);
	
	tChannel_4 = tNVP1204_600;
	for (j=0; j<54; j++)																		// 0x610 ~ 0x6E7
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x610 + j*4), tChannel_4[j]);
	
	if(pdx->m_byVideoFormat == VIDEO_FORMAT_NTSC)
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x664), 0x58585858);
	else
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x664), 0x46464646);

// 	tChannel_1 = tNVP1114_VALUE[pdx->m_byVideoFormat];
// 	for(i=0; i<2; i++)
// 	{
// 		for (j=0x00; j<0xFF; j++)
// 			I2C_Write(pdx, 1, pdx->m_byNVP1114_ID[i], (BYTE)j, tChannel_1[j]);
// 		
// 		DelayPer100nanoseconds(pdx, 100);
// 		
// 		//Set Audio buf
// 		for(j=0xA1; j<0xD0; j++)
// 			I2C_Write(pdx, 1, pdx->m_byNVP1114_ID[i], (BYTE)j, tChannel_1[j]);
// 	}
// 
// 	for(j=0; j<62; j++)
// 	{
// 		for(k=0; k<4; k++)
// 			I2C_Write(pdx, 0, 0x80, (BYTE)(k*NVP1004_DEV_OFFSET + j), tNVP1004_VALUE[pdx->m_byVideoFormat][j]);
// 	}
// 	
// 	for(j=0; j<9; j++)
// 		I2C_Write(pdx, 0, 0x80, (BYTE)(0x71 + j), tNVP1004_VALUE_B[j]);
// 	
// 	for(j=0; j<16; j++)
// 		I2C_Write(pdx, 0, 0x80, (BYTE)(0xF0 + j), tNVP1004_VALUE_C[j]);
}
//========================================================================//
BOOL GetDecoderSyncData(PDEVICE_EXTENSION pdx, BYTE byChannel, BYTE *pbyRet)
{
	BOOL	bRet = FALSE;
	
	*pbyRet = pdx->m_bSync[byChannel];
	bRet = TRUE;
	//KdPrint(("GetDecoderSyncData::ID[%X]:byChannel[%X]:bSync[%X] end\n", pdx->m_byDeviceID, byChannel, *pbyRet));
	return bRet;
}