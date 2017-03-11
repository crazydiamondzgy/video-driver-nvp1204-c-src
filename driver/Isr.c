#include "NVP1204.h"

BOOL OnInterrupt(PKINTERRUPT pInterruptObject, PDEVICE_EXTENSION pdx)
{
	BYTE	i, byNextQueue, byloss, byclamp, byfsclock;
	DWORD	m_dwAudioIntStatus, m_dwVideoIntStatus, dwLoss, dwField;
	BOOL	baudioint;

	m_dwVideoIntStatus = READ_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + VIDEO_INT_STATUS));				// video interrupt:0x1D8
	m_dwAudioIntStatus = READ_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + AUDIO_INT_STATUS));				// audio interrupt:0x228

//	KdPrint(("Video interrupt : m_dwVideoIntStatus = [%X]\n", m_dwVideoIntStatus));
//	KdPrint(("Audio interrupt : m_dwAudioIntStatus = [%X]\n", m_dwAudioIntStatus));
	pdx->m_bIntRet = FALSE;

	if(pdx->m_bACapStarted == TRUE)
	{
		if(m_dwAudioIntStatus & AD_IRQ0)
		{
			baudioint = FALSE;
			pdx->m_liIntCounter = KeQueryPerformanceCounter(NULL);
			pdx->m_liDiffCounter.QuadPart = (pdx->m_liIntCounter.QuadPart - pdx->m_liStartCounter.QuadPart) * 10000000 / pdx->m_liFrequency.QuadPart;
			if(pdx->m_liDiffCounter.QuadPart & 0x8000000000000000)
			{
				LocalTimeInit(pdx);
				pdx->m_liDiffCounter.QuadPart = 0;
			}
			pdx->m_liIntLocalTime.QuadPart = pdx->m_liStartLocalTime.QuadPart + pdx->m_liDiffCounter.QuadPart;
			
			if(pdx->m_pACAPStatus[pdx->m_byCurAudioQueueCount]->byLock == MEM_UNLOCK)
			{
				byNextQueue = pdx->m_byCurAudioQueueCount+2;
				byNextQueue %= MAX_AUDIO_QUEUE;

				if (m_dwAudioIntStatus & AD_IRQ0_A)
				{
//					KdPrint(("Audio interrupt AD_IRQ0_A:: Queue = %d\n", pdx->m_byCurAudioQueueCount));	
					WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + AUDIO_BASE_ADD0_A), pdx->m_dwAudioBuffer + byNextQueue*AUDIO_ONE_QUEUE_SIZE);
					WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + AUDIO_INT_STATUS), AD_IRQ0_A);
					baudioint = TRUE;
				}
				else if (m_dwAudioIntStatus & AD_IRQ0_B)
				{
//					KdPrint(("Audio interrupt AD_IRQ0_B:: Queue = %d\n", pdx->m_byCurAudioQueueCount));	
					WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + AUDIO_BASE_ADD0_B), pdx->m_dwAudioBuffer + byNextQueue*AUDIO_ONE_QUEUE_SIZE);
					WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + AUDIO_INT_STATUS), AD_IRQ0_B);
					baudioint = TRUE;
				}
				if(baudioint == TRUE)
				{
					pdx->m_pACAPStatus[pdx->m_byCurAudioQueueCount]->byLock = MEM_LOCK;
//					pdx->m_pACAPStatus[pdx->m_byCurAudioQueueCount]->ACAPTime.dwHighDateTime = (DWORD)(pdx->m_liIntLocalTime.QuadPart >> 32);
//					pdx->m_pACAPStatus[pdx->m_byCurAudioQueueCount]->ACAPTime.dwLowDateTime = (DWORD)pdx->m_liIntLocalTime.QuadPart;
					*pdx->m_pACapCurQCnt = pdx->m_byCurAudioQueueCount;

					pdx->m_bACapIntDone = TRUE;
					pdx->m_bIntRet = TRUE;
					KdPrint(("Audio interrupt Queue = %d\n", pdx->m_byCurAudioQueueCount));	
					pdx->m_byCurAudioQueueCount++;
					if(pdx->m_byCurAudioQueueCount >= MAX_AUDIO_QUEUE)
						pdx->m_byCurAudioQueueCount = 0;
					
					KeInsertQueueDpc(&pdx->m_DpcObjectAudio, NULL, pdx);
					baudioint = FALSE;
				}
			}
			else
			{
//				KdPrint(("MEM LOCK Queue = %d\n", pdx->m_byCurAudioQueueCount));
				WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + AUDIO_INT_STATUS), AD_IRQ0);
			}
		}
	}

	if((pdx->m_bVCapStarted == TRUE) && ((pdx->m_dwVideoIntEnable & VCAP_INT_ENABLE) != 0) && ((m_dwVideoIntStatus & VCAP_INT_ENABLE) != 0))
	{
		dwLoss = READ_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x500));			// video loss detection
		dwField = READ_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x504));		// field detection
//		KdPrint(("Capture Interrupt = [%x] \n", m_dwVideoIntStatus));
		for(i=0; i<MAX_DECODER; i++)
		{
			if(((pdx->m_dwVideoIntEnable & pdx->m_dwVideoIntMask[i]) != 0) && ((m_dwVideoIntStatus & pdx->m_dwVideoIntMask[i]) != 0))
			{
				VideoDMAStop(pdx, i);
				pdx->m_byISRChannel[i] = pdx->m_pbyChannelTable[i][pdx->m_byCurChannelCnt[i]];
				
				byloss = (BYTE)(dwLoss >> (28+i));
				if(byloss & 0x01)
					pdx->m_bSync[pdx->m_byISRChannel[i]] = FALSE;
				else
					pdx->m_bSync[pdx->m_byISRChannel[i]] = TRUE;

				if(!((dwField >> (24+i)) & 0x01))
					pdx->m_byCapField[pdx->m_byISRChannel[i]] = ODD_FIELD;
				else
					pdx->m_byCapField[pdx->m_byISRChannel[i]] = EVEN_FIELD;

// 				if(i==3)
// 					KdPrint(("ISR::Record = %d, Channel = %d, field = %d, Sync = %d, Image size = %d\n", i+1, pdx->m_byISRChannel[i]+1, 
// 						pdx->m_byCapField[pdx->m_byISRChannel[i]], pdx->m_bSync[pdx->m_byISRChannel[i]], pdx->m_byCaptureSize[pdx->m_byISRChannel[i]]));

				if(pdx->m_pVCAPStatus[i][pdx->m_byCurVideoQCount[i]]->byLock != MEM_LOCK)		// application is not busy
				{
					pdx->m_bCHSwitch[i] = FALSE;
					pdx->m_bQuSwitchSend[i] = FALSE;

					if((pdx->m_bSync[pdx->m_byISRChannel[i]] == TRUE) && (pdx->m_byISRChannel[i] != 0xff))
					{
						SetWidthSelect(pdx, i, pdx->m_byISRChannel[i]);

						if(pdx->m_byCaptureSize[pdx->m_byISRChannel[i]] != IMAGE_720X480)
						{
							if(pdx->m_byCapField[pdx->m_byISRChannel[i]] == ODD_FIELD)
							{
								pdx->m_byoddcount[pdx->m_byISRChannel[i]]++;
								if(pdx->m_bOdd[pdx->m_byISRChannel[i]] == TRUE)
								{
									if(pdx->m_byoddcount[pdx->m_byISRChannel[i]] > 1)
									{
										pdx->m_bQuSwitchSend[i] = TRUE;
										pdx->m_bCHSwitch[i] = TRUE;
										pdx->m_bOdd[pdx->m_byISRChannel[i]] = FALSE;
										pdx->m_byoddcount[pdx->m_byISRChannel[i]] = 0;
									}
								}
								else
								{
									pdx->m_bOdd[pdx->m_byISRChannel[i]] = TRUE;
								}
							}
							else if(pdx->m_byCapField[pdx->m_byISRChannel[i]] == EVEN_FIELD)
							{
								pdx->m_bOdd[pdx->m_byISRChannel[i]] = FALSE;
								pdx->m_byoddcount[pdx->m_byISRChannel[i]] = 0;
								pdx->m_bCHSwitch[i] = TRUE;
								pdx->m_bQuSwitchSend[i] = TRUE;
							}
						}
						else if(pdx->m_byCaptureSize[pdx->m_byISRChannel[i]] == IMAGE_720X480)
						{
							if(pdx->m_byCapField[pdx->m_byISRChannel[i]] == EVEN_FIELD)
							{
								pdx->m_byoddcount[pdx->m_byISRChannel[i]]++;
								if(pdx->m_bEvenSended[pdx->m_byISRChannel[i]] == FALSE)
								{
									pdx->m_bEven[pdx->m_byISRChannel[i]] = FALSE;
									pdx->m_bQuSwitchSend[i] = TRUE;
									pdx->m_bOddSended[pdx->m_byISRChannel[i]] = TRUE;

									pdx->m_bOdd[pdx->m_byISRChannel[i]] = FALSE;
									pdx->m_bEvenSended[pdx->m_byISRChannel[i]] = TRUE;
									pdx->m_byoddcount[pdx->m_byISRChannel[i]] = 0;
								}
								else
								{
									pdx->m_bEvenSended[pdx->m_byISRChannel[i]] = FALSE;
									if(pdx->m_bOdd[pdx->m_byISRChannel[i]] == TRUE)
									{
										if(pdx->m_byoddcount[pdx->m_byISRChannel[i]] > 1)
										{
											pdx->m_bQuSwitchSend[i] = TRUE;
											pdx->m_bCHSwitch[i] = TRUE;
											pdx->m_bOddSended[pdx->m_byISRChannel[i]] = FALSE;
											
											pdx->m_bEven[pdx->m_byISRChannel[i]] = FALSE;
											pdx->m_bOdd[pdx->m_byISRChannel[i]] = FALSE;
											pdx->m_byoddcount[pdx->m_byISRChannel[i]] = 0;
										}
									}
									else
									{
										pdx->m_bOdd[pdx->m_byISRChannel[i]] = TRUE;
									}
								}
							}
							else if(pdx->m_byCapField[pdx->m_byISRChannel[i]] == ODD_FIELD)
							{
								pdx->m_byevencount[pdx->m_byISRChannel[i]]++;
								pdx->m_bEvenSended[pdx->m_byISRChannel[i]] = FALSE;
								if(pdx->m_bOddSended[pdx->m_byISRChannel[i]] == TRUE)
								{
									pdx->m_bQuSwitchSend[i] = TRUE;
									pdx->m_bCHSwitch[i] = TRUE;
									pdx->m_bOddSended[pdx->m_byISRChannel[i]] = FALSE;

									pdx->m_bEven[pdx->m_byISRChannel[i]] = FALSE;
									pdx->m_bOdd[pdx->m_byISRChannel[i]] = FALSE;
									pdx->m_byevencount[pdx->m_byISRChannel[i]] = 0;
								}
								else
								{
									if(pdx->m_bEven[pdx->m_byISRChannel[i]] == TRUE)
									{
										if(pdx->m_byevencount[pdx->m_byISRChannel[i]] > 1)
										{
											pdx->m_bQuSwitchSend[i] = TRUE;
											pdx->m_bCHSwitch[i] = TRUE;
											pdx->m_bOddSended[pdx->m_byISRChannel[i]] = FALSE;

											pdx->m_bEven[pdx->m_byISRChannel[i]] = FALSE;
											pdx->m_bOdd[pdx->m_byISRChannel[i]] = FALSE;
											pdx->m_byevencount[pdx->m_byISRChannel[i]] = 0;
										}
									}
									else
										pdx->m_bEven[pdx->m_byISRChannel[i]] = TRUE;
								}
							}
						}
						if(pdx->m_bCHSwitch[i] == TRUE)
							SetCameraChange(pdx, i);

						ColorControl(pdx, i);

						pdx->m_byNextVideoQCount[i] = pdx->m_byCurVideoQCount[i] + 1;
						if(pdx->m_byNextVideoQCount[i] >= MAX_VIDEO_QUEUE)
							pdx->m_byNextVideoQCount[i] = 0;
					}
					else
					{
						if(i==3)
							KdPrint(("ISR::Video Record 3 is 0xFF\n"));

						if(pdx->m_byCaptureSize[pdx->m_byISRChannel[i]] != IMAGE_720X480)
						{
							if(pdx->m_byCapField[pdx->m_byISRChannel[i]] == EVEN_FIELD)
								SetCameraChange(pdx, i);
						}
						else
							SetCameraChange(pdx, i);
					}

					if(pdx->m_bQuSwitchSend[i] == TRUE)				// video memory update
					{
						pdx->m_byOldVideoQCount[i] = pdx->m_byCurVideoQCount[i];
						pdx->m_byCurVideoQCount[i] = pdx->m_byNextVideoQCount[i];
						
						WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + YBASE_ADD0_A + i*0x20), pdx->m_dwVideoBuffer[i] + pdx->m_byCurVideoQCount[i]*pdx->m_dwOneQueueSize);
						if(pdx->m_byDataFormat != DATAFORMAT_YUY422PK)
						{
							WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + YBASE_ADD0_A_U + i*0x20), pdx->m_dwVideoBuffer[i] + pdx->m_byCurVideoQCount[i]*pdx->m_dwOneQueueSize + pdx->m_dwYOffSetSize); 
							WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + YBASE_ADD0_A_V + i*0x20), pdx->m_dwVideoBuffer[i] + pdx->m_byCurVideoQCount[i]*pdx->m_dwOneQueueSize + pdx->m_dwYOffSetSize + pdx->m_dwUVOffSetSize);
						}
						pdx->m_bVCapIntDone[i] = TRUE;
					}
					WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + VIDEO_INT_STATUS), pdx->m_dwVideoIntMask[i]);			// 0x1D8
					VideoDMAStart(pdx, i);
//					KdPrint(("VideoDMAStart::m_dwCapDMAEnable[%X], m_dwCapIntEnable[%X], Field[%X]\n", pdx->m_dwCapDMAEnable, pdx->m_dwCapIntEnable, pdx->m_byCapField[pdx->m_byISRChannel[i]]));
				}
				else			// MEM_LOCK : application is busy
				{
					KdPrint(("Driver::Capture MEM_LOCK::Record Path=[%d], Channel=%d, Queue = %d\n",
							i+1, pdx->m_byISRChannel[i]+1, pdx->m_byOldVideoQCount[i]));
					WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + VIDEO_INT_STATUS), pdx->m_dwVideoIntMask[i]);			// 0x1D8
					VideoDMAStart(pdx, i);
				}

				if(pdx->m_bVCapIntDone[i] == TRUE)
				{
					pdx->m_liIntCounter = KeQueryPerformanceCounter(NULL);
					pdx->m_liDiffCounter.QuadPart = (pdx->m_liIntCounter.QuadPart - pdx->m_liStartCounter.QuadPart) * 10000000 / pdx->m_liFrequency.QuadPart;
					if(pdx->m_liDiffCounter.QuadPart & 0x8000000000000000)
					{
						LocalTimeInit(pdx);
						pdx->m_liDiffCounter.QuadPart = 0;
					}
					pdx->m_liIntLocalTime.QuadPart = pdx->m_liStartLocalTime.QuadPart + pdx->m_liDiffCounter.QuadPart;

					pdx->m_pVCAPStatus[i][pdx->m_byOldVideoQCount[i]]->byLock		= MEM_LOCK;
					pdx->m_pVCAPStatus[i][pdx->m_byOldVideoQCount[i]]->bySize		= pdx->m_byCaptureSize[pdx->m_byISRChannel[i]];
					pdx->m_pVCAPStatus[i][pdx->m_byOldVideoQCount[i]]->byField		= pdx->m_byCapField[pdx->m_byISRChannel[i]];
					pdx->m_pVCAPStatus[i][pdx->m_byOldVideoQCount[i]]->VCAPTime.dwHighDateTime	= (DWORD)(pdx->m_liIntLocalTime.QuadPart >> 32);
					pdx->m_pVCAPStatus[i][pdx->m_byOldVideoQCount[i]]->VCAPTime.dwLowDateTime	= (DWORD)pdx->m_liIntLocalTime.QuadPart;
					*pdx->m_pVCapCurQCnt[i] = pdx->m_byOldVideoQCount[i];
					if(pdx->m_byRealTime == TRUE)
						pdx->m_pVCAPStatus[i][pdx->m_byOldVideoQCount[i]]->byChannel = pdx->m_byISRChannel[i]%MAX_DECODER;
					else
						pdx->m_pVCAPStatus[i][pdx->m_byOldVideoQCount[i]]->byChannel = pdx->m_byISRChannel[i];
					KeInsertQueueDpc(&pdx->m_DpcObjectVideo, NULL, pdx);
					if(i==3)
						KdPrint(("ISR::Video Record 3 DMA Completed::\n"));

				}
			}
		}
		pdx->m_bIntRet = TRUE;
	}
	return pdx->m_bIntRet;
}
//=================================================================================//
void RealTimeMode(PDEVICE_EXTENSION pdx, BYTE bydecoder)
{
	DWORD	dwMUXData;
	
	dwMUXData = READ_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + MUX_OUT));
	dwMUXData = (dwMUXData & 0xffffff00);
	WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + MUX_DIR), MUX_DIR_OUT);
	WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + MUX_OUT), dwMUXData);

	if(pdx->m_byVideoFormat == VIDEO_FORMAT_NTSC)
	{
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x520), 0x4000AB02);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x530), 0x4000AB02);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x540), 0x4000AB02);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x550), 0x4000AB02);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x564), 0x9F002040);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x568), 0x8050380F);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x574), 0x2accf02f);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x578), 0x57431088);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x57C), 0x82630100);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x580), 0x80000081);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x584), 0x01000000);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x588), 0x0020042E);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x58C), 0x0030B801);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x59C), 0x0322FF00);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x644), 0xffffffef);
	}
	else
	{
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x520), 0xbd00be00);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x530), 0xbd00be00);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x540), 0xbd00be00);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x550), 0xbd00be00);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x564), 0x9F002040);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x570), 0x89238804);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x574), 0x2ACCF02F);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x578), 0x57431088);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x580), 0x80000081);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x584), 0x01000000);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x588), 0x0020042E);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x58C), 0x0030D801);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x59C), 0x0322FF00);
		WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x644), 0xffffffef);
	}
}
//=================================================================================//
void SetCameraChange(PDEVICE_EXTENSION pdx, BYTE bydecoder)
{
	pdx->m_byNextChannelCnt[bydecoder] = pdx->m_byCurChannelCnt[bydecoder];
	pdx->m_byNextChannelCnt[bydecoder]++;
	if(pdx->m_byNextChannelCnt[bydecoder] >= pdx->m_byMaxChannel)			// m_byMaxChannelLen=30 : if NTSC, m_byMaxChannelLen=25 : if PAL
		pdx->m_byNextChannelCnt[bydecoder] = 0;
	
	if(bydecoder==3)
		KdPrint(("Driver Camera change::Next Channel=%d\n", pdx->m_pbyChannelTable[bydecoder][pdx->m_byNextChannelCnt[bydecoder]]+1));
	
	pdx->m_byCurChannelCnt[bydecoder] = pdx->m_byNextChannelCnt[bydecoder];
}
//=================================================================================//
void SetCameraColor(PDEVICE_EXTENSION pdx, BYTE bydecoder, BYTE bychannel)
{
	DWORD	dwTemp, dwcolordata;

	if(pdx->m_bDecoderBW[bydecoder] != pdx->m_bVideoBW[bychannel])
	{
		if(pdx->m_bVideoBW[bychannel] == 0x1)
		{
			dwTemp = READ_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x598));
			dwcolordata = (0x6<<(bydecoder*4)) | (dwTemp & (~(0xf << (bydecoder*4))));
			WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x598), dwcolordata);
		}
		else if(pdx->m_bVideoBW[bychannel] == 0x0)
		{
			dwTemp = READ_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x598));
			dwcolordata = (0x3<<(bydecoder*4)) | (dwTemp & (~(0xf << (bydecoder*4))));
			WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + 0x598), dwcolordata);
		}
		pdx->m_bDecoderBW[bydecoder] = pdx->m_bVideoBW[bychannel];
	}
}
//=================================================================================//
void SetWidthSelect(PDEVICE_EXTENSION pdx, BYTE byRecPath, BYTE byChannel)
{
	DWORD	dwData=0, dwDMASize, dwVideoHeight;	

	if((pdx->m_byVideoFormat == VIDEO_FORMAT_NTSC) || ((pdx->m_byVideoFormat == VIDEO_FORMAT_PAL) && (pdx->m_byVideoVScale == IMAGEHEIGHT_480)))
		dwVideoHeight = 240;
	else
		dwVideoHeight = 288;

	dwDMASize = pdx->m_wImageSize[byChannel]*dwVideoHeight*2/sizeof(DWORD);
	dwData = (pdx->m_wImageSize[byChannel] << 8) | (0x1 << pdx->m_byDataFormat);
	WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO0_FORMAT + byRecPath*0x04), dwData);				// 0x190
	WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_DMA_SIZE0 + byRecPath*0x04), dwDMASize);		// 0x1C0

//	KdPrint(("SetWidthSelect:: m_wImageSize = [%x], m_wImageHeight = %d\n", pdx->m_wImageSize[byChannel], pdx->m_wImageHeight));

	if(pdx->m_byField_HWidth == HPIXEL_640_MODE)															// 640 Pixel
	{
		if(pdx->m_byCaptureSize[byChannel] == IMAGE_360X240)												// Image Size : CIF
			dwData = 0x74000;																				// 320 x 240
		else																								// Image Size : Field or Frame
			dwData = 0xE8000;																				// 640 x 240
	}
	else																									// 704 or 720 Pixel
	{
		if(pdx->m_byCaptureSize[byChannel] == IMAGE_360X240)												// Image Size : CIF
			dwData = 0x80000;																				// 360 x 240
		else																								// Image Size : Field or Frame
			dwData = 0x100000;																				// 704 x 240
	}

	WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_HSCALE1 + byRecPath*0x04), dwData);				// 0x620

	if((pdx->m_byVideoFormat == VIDEO_FORMAT_PAL) && (pdx->m_byVideoVScale == IMAGEHEIGHT_480))
		WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_VSCALE1 + byRecPath*0x04), 0xD4800);		// 0x630
	else
		WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_VSCALE1 + byRecPath*0x04), 0x100000);		// 0x630
}
//=================================================================================//
BOOL ColorControl(PDEVICE_EXTENSION pdx, BYTE byDecoder)
{
	DWORD	dwData;

	if(pdx->m_byRecPathBrightness[byDecoder] != pdx->m_byBrightness[byDecoder])
	{
		pdx->m_byRecPathBrightness[byDecoder] = pdx->m_byBrightness[byDecoder];
		dwData = READ_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + NVP1204_BRIGHT + 0x10*byDecoder));
		dwData &= 0xff00ffff;
		dwData |= (pdx->m_byBrightness[byDecoder] << 16);
		WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + NVP1204_BRIGHT + 0x10*byDecoder), dwData);
	}
	if(pdx->m_byRecPathContrast[byDecoder] != pdx->m_byContrast[byDecoder])
	{
		pdx->m_byRecPathContrast[byDecoder] = pdx->m_byContrast[byDecoder];
		dwData = READ_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + NVP1204_CONTRAST + 0x10*byDecoder));
		dwData &= 0xffff00ff;
		dwData |= (pdx->m_byContrast[byDecoder] << 8);
		WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + NVP1204_CONTRAST + 0x10*byDecoder), dwData);
	}
	if(pdx->m_byRecPathColor[byDecoder] != pdx->m_byColor[byDecoder])
	{
		pdx->m_byRecPathColor[byDecoder] = pdx->m_byColor[byDecoder];
		dwData = READ_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + NVP1204_COLOR + 0x10*byDecoder));
		dwData &= 0x00ffffff;
		dwData |= (pdx->m_byColor[byDecoder] << 24);
		WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + NVP1204_COLOR + 0x10*byDecoder), dwData);
	}
	if(pdx->m_byRecPathHue[byDecoder] != pdx->m_byHue[byDecoder])
	{
		pdx->m_byRecPathHue[byDecoder] = pdx->m_byHue[byDecoder];
		dwData = READ_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + NVP1204_HUE + 0x10*byDecoder));
		dwData &= 0xffffff00;
		dwData |= pdx->m_byHue[byDecoder];
		WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + NVP1204_HUE + 0x10*byDecoder), dwData);
	}
	return TRUE;
}
//=================================================================================//
void AudioDMAStart(PDEVICE_EXTENSION pdx, BYTE byRecPath)
{
	WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + AUDIO_DMA_ENABLE), pdx->m_dwAudioDMAEnable);
	WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + AUDIO_INT_ENABLE), pdx->m_dwAudioIntEnable);
}
//=================================================================================//
void AudioDMAStop(PDEVICE_EXTENSION pdx, BYTE byRecPath)
{
	WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + AUDIO_DMA_ENABLE), pdx->m_dwAudioDMAEnable);
	WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + AUDIO_INT_ENABLE), pdx->m_dwAudioIntEnable);
}
//=================================================================================//
void VideoDMAStart(PDEVICE_EXTENSION pdx, BYTE byRecPath)
{
	pdx->m_dwVideoDMAEnable |= 0x0000000f & pdx->m_dwVideoDMAMask[byRecPath];
	pdx->m_dwVideoIntEnable |= 0x00000055 & pdx->m_dwVideoIntMask[byRecPath];

	WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + VIDEO_DMA_ENABLE), pdx->m_dwVideoDMAEnable);		// 0x1D0
	WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + VIDEO_INT_ENABLE), pdx->m_dwVideoIntEnable);		// 0x1D4
}
//=================================================================================//
void VideoDMAStop(PDEVICE_EXTENSION pdx, BYTE byRecPath)
{
	pdx->m_dwVideoDMAEnable &= 0xfffffff0 | ~pdx->m_dwVideoDMAMask[byRecPath];
	pdx->m_dwVideoIntEnable &= 0xffffff00 | ~pdx->m_dwVideoIntMask[byRecPath];

	WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + VIDEO_DMA_ENABLE), pdx->m_dwVideoDMAEnable);
	WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + VIDEO_INT_ENABLE), pdx->m_dwVideoIntEnable);
}
//=================================================================================//
VOID DpcForIsr_Video(PKDPC Dpc, PDEVICE_OBJECT pDeviceObject, PIRP junk, PDEVICE_EXTENSION pdx)
{
	BYTE	i;
	
	if(pdx->m_bVCapStarted == TRUE)
	{
		for(i=0; i<MAX_DECODER; i++)
		{
			if(pdx->m_bVCapIntDone[i] == TRUE)
			{
				KeSetEvent(pdx->m_pVideoEvent[i], (KPRIORITY)0, FALSE);
				pdx->m_bVCapIntDone[i] = FALSE;
				//KdPrint(("DpcForIsr_Video::pdx->m_bVCapIntDone[%d] = %X\n", i, pdx->m_bVCapIntDone[i]));
			}
		}
	}
}
//=================================================================================//
VOID DpcForIsr_Audio(PKDPC Dpc, PDEVICE_OBJECT pDeviceObject, PIRP junk, PDEVICE_EXTENSION pdx)
{
	if(pdx->m_bACapStarted == TRUE)
	{
		if(pdx->m_bACapIntDone == TRUE)
		{					
			KeSetEvent(pdx->m_pAudioEvent, (KPRIORITY)0, FALSE);
			pdx->m_bACapIntDone = FALSE;
			//KdPrint(("DpcForIsr_Audio::pdx->m_bACapIntDone = %X\n", pdx->m_bACapIntDone));	
		}
	}
}