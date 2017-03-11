#include "NVP1204.h"


//========================================================================//
void SetLocalRegister(PDEVICE_EXTENSION pdx)
{
	DWORD	i, j, dwskipData = 4, dwVideoCapSize, dwData, *tChannel_4;
	WORD	wImageRWidth[MAX_IMAGEWIDTH][MAX_HSCALE] = {{IMAGEWIDTH_R320, IMAGEWIDTH_R352, IMAGEWIDTH_R360}, {IMAGEWIDTH_R640, IMAGEWIDTH_R704, IMAGEWIDTH_R720}};
	WORD	wImageRHeight[MAX_VIDEO_FORMAT] = {IMAGEHEIGHT_R480, IMAGEHEIGHT_R576};

	pdx->m_dwOneQueueSize = IMAGEWIDTH_R720*(IMAGEHEIGHT_R576/2)*2;			// 720x576/2x2
	pdx->m_dwYOffSetSize  = IMAGEWIDTH_R720*(IMAGEHEIGHT_R576/2);			// 720x576/2
	pdx->m_dwUVOffSetSize = IMAGEWIDTH_R720*(IMAGEHEIGHT_R576/2)/4;			// 720x576/2/4

	for(i=0; i<MAX_DECODER; i++)
	{
		for(j=0; j<MAX_VIDEO_QUEUE; j++)
			pdx->m_pVCAPStatus[i][j] = (VCAP_STATUS_INFO *)(pdx->m_pbyVideoBuffer[i] + MAX_VIDEO_CAP_SIZE*PAGESIZE - ((j+1)*sizeof(VCAP_STATUS_INFO) + 1));

		pdx->m_pVCapCurQCnt[i] = (BYTE *)(pdx->m_pbyVideoBuffer[i] + MAX_VIDEO_CAP_SIZE*PAGESIZE - ((MAX_VIDEO_QUEUE+1)*sizeof(VCAP_STATUS_INFO) + 1));
	}

	for(j=0; j<MAX_AUDIO_QUEUE; j++)
		pdx->m_pACAPStatus[j] = (ACAP_STATUS_INFO *)(pdx->m_pbyAudioBuffer + MAX_AUDIO_CAP_SIZE*PAGESIZE - ((j+1)*sizeof(ACAP_STATUS_INFO) + 1));
	
	pdx->m_pACapCurQCnt = (BYTE *)(pdx->m_pbyAudioBuffer + MAX_AUDIO_CAP_SIZE*PAGESIZE - ((MAX_AUDIO_QUEUE+1)*sizeof(ACAP_STATUS_INFO) + 1));

	for(i=0; i<MAX_VIDEO_CHANNEL; i++)
	{
//		KdPrint(("pdx->m_byImageSize[%d] = %X\n", i, pdx->m_byCaptureSize[i]));
		if(pdx->m_byCaptureSize[i] == IMAGE_360X240)
			pdx->m_wImageSize[i] = wImageRWidth[IMAGEWIDTH_320_360][pdx->m_byField_HWidth];
		else
			pdx->m_wImageSize[i] = wImageRWidth[IMAGEWIDTH_640_720][pdx->m_byField_HWidth];
	}

	for(i=0; i<MAX_DECODER; i++)
	{
		WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + YBASE_ADD0_A + i*0x20), pdx->m_dwVideoBuffer[i]);		// 0x100

		if(pdx->m_byDataFormat != DATAFORMAT_YUY422PK)
		{
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + YBASE_ADD0_A_U + i*0x20), pdx->m_dwVideoBuffer[i] + pdx->m_dwYOffSetSize);		// 0x104
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + YBASE_ADD0_A_V + i*0x20), pdx->m_dwVideoBuffer[i] + pdx->m_dwYOffSetSize + pdx->m_dwUVOffSetSize);
		}

		pdx->m_dwVideoFormat[i] &= 0xFFFFFFF8;
		pdx->m_dwVideoFormat[i] |= (0x1 << pdx->m_byDataFormat);

		dwData = (pdx->m_wImageSize[i] << 8) | pdx->m_dwVideoFormat[i];
		WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO0_FORMAT + i*0x04), dwData);					// 0x190

		dwVideoCapSize = (DWORD)(pdx->m_wImageSize[i]*pdx->m_wImageHeight*2)/sizeof(DWORD);
		WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_DMA_OFFSET0 + i*0x04), dwskipData);			// 0x1B0
		WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_DMA_SIZE0 + i*0x04), dwVideoCapSize);		// 0x1C0

		if(pdx->m_byField_HWidth == HPIXEL_640_MODE)														// 640 Pixel
		{
			if(pdx->m_byCaptureSize[i] == IMAGE_360X240)													// Image Size : CIF
				dwData = 0x74000;																			// 320 x 240
			else																							// Image Size : Field or Frame
				dwData = 0xE8000;																			// 640 x 240
		}
		else																								// 704 or 720 Pixel
		{
			if(pdx->m_byCaptureSize[i] == IMAGE_360X240)													// Image Size : CIF
				dwData = 0x80000;																			// 360 x 240
			else																							// Image Size : Field or Frame
				dwData = 0x100000;																			// 704 x 240
		}
		WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_HSCALE1 + i*0x04), dwData);					// 0x620
		
		if((pdx->m_byVideoFormat == VIDEO_FORMAT_NTSC) || ((pdx->m_byVideoFormat == VIDEO_FORMAT_PAL) && (pdx->m_byVideoVScale == IMAGEHEIGHT_480)))
		{
			pdx->m_wImageHeight = wImageRHeight[0]/2;
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_VSCALE1 + i*0x04), 0xD4800);			// 0x630
		}
		else
		{
			pdx->m_wImageHeight = wImageRHeight[pdx->m_byVideoFormat]/2;
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_VSCALE1 + i*0x04), 0x100000);			// 0x630
		}
	}
	
	if(pdx->m_pbyAudioBuffer)
	{
		WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_BASE_ADD0_A), pdx->m_dwAudioBuffer);		// 0x200
		WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_BASE_ADD0_B), pdx->m_dwAudioBuffer+AUDIO_ONE_QUEUE_SIZE);		// 0x204
	}
	
	WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_DMA_ENABLE), ALL_CAP_DMA_DISABLE);						// 0x1D0
	WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_INT_ENABLE), ALL_CAP_INT_DISABLE);						// 0x1D4
	
	WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_DMA_ENABLE), ALL_CAP_DMA_DISABLE);						// 0x220
	WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_INT_ENABLE), ALL_CAP_INT_DISABLE);						// 0x224
	WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_DMA_SIZE0), CUR_AUDIO_PACKET_LENGTH*8/sizeof(DWORD));	// 0x210

	WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x1A0), 0x00008000);
}
//========================================================================//
void DelayPer100nanoseconds(PDEVICE_EXTENSION pdx, DWORD DelayTime)
{
	DWORD	dwFirstHiTime, dwFirstLowTime;
	DWORD	dwLastHiTime,  dwLastLowTime;
	DWORD	dwDiffTime;
	
	GetCurrentSystemTime(&dwFirstHiTime, &dwFirstLowTime);	
	while(TRUE)
	{
		GetCurrentSystemTime(&dwLastHiTime, &dwLastLowTime);
		dwDiffTime = dwLastLowTime - dwFirstLowTime;
		if(dwDiffTime > DelayTime)
			break;
	}
}
//========================================================================//
void	GetCurrentSystemTime(DWORD* pdwHiTime, DWORD* pdwLowTime)
{
	ULONGLONG	ticks, rate;
	
	ticks = (ULONGLONG)KeQueryPerformanceCounter((PLARGE_INTEGER)&rate).QuadPart;
	ticks = (ticks & 0xFFFFFFFF00000000) / rate * 10000000 + (ticks & 0x00000000FFFFFFFF) * 10000000 / rate;
	*pdwHiTime = (DWORD)(ticks >> 32) & 0xFFFFFFFF;
	*pdwLowTime = (DWORD)ticks & 0xFFFFFFFF;
}
//========================================================================//
BOOL LocalTimeInit(PDEVICE_EXTENSION pdx)
{
	LARGE_INTEGER	StartSystemTime, StartLocalTime, StartFrequency;
	LARGE_INTEGER	EndSystemTime, EndLocalTime, EndFrequency, StartCounter;
	TIME_FIELDS		StartTimeFields, EndTimeFields;
	BYTE			i;
	BOOL			bRet = FALSE;

	KeQuerySystemTime(&StartSystemTime);	//Sometimes Error -> why return value void ? Microsoft bug
	ExSystemTimeToLocalTime(&StartSystemTime, &StartLocalTime);
	RtlTimeToTimeFields(&StartLocalTime, &StartTimeFields);
	StartCounter = KeQueryPerformanceCounter(&StartFrequency);

	for(i=0; i<100; i++)
	{
		KeQuerySystemTime(&EndSystemTime);
		StartCounter = KeQueryPerformanceCounter(&EndFrequency);
		ExSystemTimeToLocalTime(&EndSystemTime, &EndLocalTime);
		RtlTimeToTimeFields(&EndLocalTime, &EndTimeFields);
		if((StartTimeFields.Year == EndTimeFields.Year) && (StartTimeFields.Month == EndTimeFields.Month) && 
			(StartTimeFields.Day == EndTimeFields.Day) && (StartTimeFields.Hour == EndTimeFields.Hour) &&
			(StartTimeFields.Minute == EndTimeFields.Minute) && (StartFrequency.QuadPart == EndFrequency.QuadPart))
		{
			pdx->m_liStartCounter.QuadPart = StartCounter.QuadPart;
			pdx->m_liFrequency.QuadPart = EndFrequency.QuadPart;
			pdx->m_liStartLocalTime.QuadPart = EndLocalTime.QuadPart;
			bRet = TRUE;
			break;
		}
		else
		{
			memcpy((BYTE *)&StartTimeFields, (BYTE *)&EndTimeFields, sizeof(TIME_FIELDS));
			StartFrequency.QuadPart = EndFrequency.QuadPart;
		}
	}
	return bRet;
}