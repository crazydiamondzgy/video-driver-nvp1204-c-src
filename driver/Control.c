#include "NVP1204.h"

//extern BYTE	tMaxDisplayMode[MAX_DISPLAY_MODE];
//=================================================================================//
NTSTATUS AL_DispatchControl(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	NTSTATUS			Status = STATUS_SUCCESS;
	PIO_STACK_LOCATION	stack;
	PDEVICE_EXTENSION	pdx;

	BYTE			byChannel, Frame_Channel, i, j, byloss;
	BYTE			byDevice, byRecPath, byOffset, byData;
	BYTE			byInBuf[32], byOutBuf[32];	
	DWORD			dwLocalAddress, dwLocalData, dwLoss;
	DWORD			dwRetSize = 0, dwInSize, dwOutSize, dwCode;
	WORD			wLocalAddress, wLocalData, wIndex, wInBuf[4];		
	int				Record_Frame[4];
	HANDLE			hEvent;
	
	pdx		= (PDEVICE_EXTENSION)pDeviceObject->DeviceExtension;
	Status	= IoAcquireRemoveLock(&pdx->m_RemoveLock, pIrp);
	
	if (!NT_SUCCESS(Status))
		return CompleteRequest(pIrp, Status, 0);	
	
	stack		= IoGetCurrentIrpStackLocation(pIrp);
	dwInSize	= stack->Parameters.DeviceIoControl.InputBufferLength;
	dwOutSize	= stack->Parameters.DeviceIoControl.OutputBufferLength;
	dwCode		= stack->Parameters.DeviceIoControl.IoControlCode;
	
	switch(dwCode)
	{
		case IOCTL_GET_ALL_VIDEO_SYNC:
			for(i=0; i<MAX_VIDEO_CHANNEL; i++)
			{
				if(GetDecoderSyncData(pdx, i, &byData) == TRUE)
					((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[i] = byData;
			}
			dwRetSize = sizeof(BYTE)*MAX_VIDEO_CHANNEL;
			break;

		case IOCTL_SET_VIDEO_EVENT:
			for(i=0; i<MAX_DECODER; i++)
			{
				hEvent = (HANDLE)((DWORD *)pIrp->AssociatedIrp.SystemBuffer)[i];
				if(!hEvent)
				{
					KdPrint(("R3 Event Error \n"));
					Status = STATUS_INVALID_PARAMETER;
				}
				else
				{
					Status = ObReferenceObjectByHandle(hEvent, SYNCHRONIZE, *ExEventObjectType, pIrp->RequestorMode, (PVOID *)&pdx->m_pVideoEvent[i], NULL);
					if(!NT_SUCCESS(Status))
						KdPrint(("ObReferenceObjectByHandle fail \n"));
				}
			}
			break;

		case IOCTL_SET_AUDIO_EVENT:
			hEvent = (HANDLE)((DWORD *)pIrp->AssociatedIrp.SystemBuffer)[0];
			if(!hEvent)
			{
				KdPrint(("R3 Event Error \n"));
				Status = STATUS_INVALID_PARAMETER;
			}
			else
			{
				Status = ObReferenceObjectByHandle(hEvent, SYNCHRONIZE, *ExEventObjectType, pIrp->RequestorMode, (PVOID *)&pdx->m_pAudioEvent, NULL);
				if(!NT_SUCCESS(Status))
					KdPrint(("ObReferenceObjectByHandle fail \n"));
			}
			break;

		case IOCTL_VIDEO_BUFFER_MAPPING:
			KdPrint(("IOCTL_VIDEO_BUFFER_MAPPING :: [%8X]\n", (DWORD)pdx->m_pbyUserVideoBuffer[0]));
			for(i=0; i<MAX_DECODER; i++)
			{
				((DWORD *)pIrp->AssociatedIrp.SystemBuffer)[i] = (DWORD)pdx->m_pbyUserVideoBuffer[i];
			}
			dwRetSize = sizeof(DWORD)*MAX_DECODER;
			break;

		case IOCTL_AUDIO_BUFFER_MAPPING:
//			KdPrint(("IOCTL_AUDIO_BUFFER_MAPPING :: [%8X]\n", (DWORD)pdx->m_pbyUserAudioBuffer[0]));

			((DWORD *)pIrp->AssociatedIrp.SystemBuffer)[0] = (DWORD)pdx->m_pbyUserAudioBuffer;
			dwRetSize = sizeof(DWORD)*1;
			break;
			
		case IOCTL_ENCODER_BUFFER_MAPPING:
//			KdPrint(("IOCTL_ENCODER_BUFFER_MAPPING :: [%8X]\n", (DWORD)pdx->m_pbyUserEncoderBuffer[0]));
			for(i=0; i<MAX_DECODER; i++)
			{
				((DWORD *)pIrp->AssociatedIrp.SystemBuffer)[i] = (DWORD)pdx->m_pbyUserEncoderBuffer[i];
			}
			dwRetSize = sizeof(DWORD)*MAX_DECODER;
			break;

		case IOCTL_DEVICE_ALL_RESET:
			pdx->m_byVideoFormat	= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[0];
			pdx->m_byField_HWidth	= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[1];
			pdx->m_byDataFormat		= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[2];		

	//		KdPrint(("IOCTL_DEVICE_ALL_RESET::m_byDeviceID[%d]VideoFormat[%d]:HS[%d]:DM[%d]\n",pdx->m_byDeviceID, pdx->m_byVideoFormat, pdx->m_byField_HWidth, pdx->m_byDataFormat, pdx->m_byDisplayMode));
			
			AllDMADisable(pdx);
			SetLocalRegister(pdx);
			I2C_Init(pdx);
			Decoder_Init(pdx);
			break;

		case IOCTL_SET_FRAMECOUNT:
			Frame_Channel						= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[0];		// Channel
			pdx->m_byFrame_Count[Frame_Channel] = ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[1];		// Frame Count
			pdx->m_byCaptureSize[Frame_Channel]	= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[2];		// Image Size
//			KdPrint(("IOCTL_SET_FRAMECOUNT::Frame_Channel = [%d], m_byImageSize = [%d]\n", Frame_Channel, pdx->m_byCaptureSize[Frame_Channel]));
			break;

		case IOCTL_SET_REALTIME:
			pdx->m_byRealTime = ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[0];
			memset(pdx->m_bReal_Time, pdx->m_byRealTime, sizeof(pdx->m_bReal_Time));
			break;

		case IOCTL_VIDEO_INIT_PARAMETER:
			KdPrint(("IOCTL_VIDEO_INIT_PARAMETER\n"));
			pdx->m_byVideoFormat	= (BYTE)(((DWORD *)pIrp->AssociatedIrp.SystemBuffer)[0]);
			pdx->m_byField_HWidth	= (BYTE)(((DWORD *)pIrp->AssociatedIrp.SystemBuffer)[1]);
			pdx->m_byDataFormat		= (BYTE)(((DWORD *)pIrp->AssociatedIrp.SystemBuffer)[2]);
			pdx->m_byVideoVScale    = (BYTE)(((DWORD *)pIrp->AssociatedIrp.SystemBuffer)[3]);
			
			AllDMADisable(pdx);
			SetLocalRegister(pdx);
			I2C_Init(pdx);
			Decoder_Init(pdx);
			
			KdPrint(("IOCTL_INIT_PARAMETER_CAPTURE::nCH[%d]:m_byVideoFormat[%d]:m_byDataFormat[%d]:m_byField_HWidth[%d]\n",
					pdx->m_byDeviceID, pdx->m_byVideoFormat, pdx->m_byDataFormat, pdx->m_byField_HWidth));
			break;	
			
		case IOCTL_AUDIO_INIT_PARAMETER:
			byInBuf[0] = ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[0];			// Sampling Frequency
			
			pdx->m_bSampleFrequency = (BOOL)byInBuf[0];

			if(pdx->m_bSampleFrequency == 0)												// 8KHz
				WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + 0x6A4), 0xD2F00303);
			else if(pdx->m_bSampleFrequency == 1)											// 16KHz
				WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + 0x6A4), 0x69780303);
			
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + 0x6A8), 0x000010FF);
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + 0x6A8), 0x000050FF);

			// Audio initialize
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_DMA_ENABLE), 0x0);		// 0x220
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_INT_ENABLE), 0x0);		// 0x224
			
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_DMA_ENABLE), 0x1);		// 0x220
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_INT_ENABLE), 0x3);		// 0x224
			
			DelayPer100nanoseconds(pdx, 1000);
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_DMA_ENABLE), 0x0);		// 0x220
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_INT_ENABLE), 0x0);		// 0x224
			
			break;

		case IOCTL_CAPTURE_START:
			byData = ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[0];		// Channel

			KdPrint(("IOCTL_CAPTURE_START::m_byDeviceID[%d]:m_dwPhysDisplayAddress[%X]\n", pdx->m_byDeviceID, pdx->m_dwPhysDisplayAddress));
			memset(pdx->m_byCurChannelCnt, 0x00, sizeof(pdx->m_byCurChannelCnt));
			memset(pdx->m_byNextChannelCnt, 0x00, sizeof(pdx->m_byNextChannelCnt));
			memset(pdx->m_bVCapIntDone, 0x00, sizeof(pdx->m_bVCapIntDone));
			memset(Record_Frame, 0x00, sizeof(Record_Frame));
			memset(pdx->m_byNextVideoQCount, 0x00, sizeof(pdx->m_byNextVideoQCount));		
			memset(pdx->m_byCurVideoQCount, 0x00, sizeof(pdx->m_byCurVideoQCount));
			memset(pdx->m_byOldVideoQCount, 0x00, sizeof(pdx->m_byOldVideoQCount));	
			memset(pdx->m_bOddSended, 0x00, sizeof(pdx->m_bOddSended));
			memset(pdx->m_bEvenSended, 0x00, sizeof(pdx->m_bEvenSended));
			memset(pdx->m_byevencount, 0x0, sizeof(pdx->m_byevencount));
			memset(pdx->m_byoddcount, 0x0, sizeof(pdx->m_byoddcount));
			memset(pdx->m_bOdd, 0x00, sizeof(pdx->m_bOdd));
			memset(pdx->m_bEven, 0x0, sizeof(pdx->m_bEven));
			memset(pdx->m_bLoss, 0x0, sizeof(pdx->m_bLoss));
			memset(pdx->dwLowDateTime, 0x0, sizeof(pdx->dwLowDateTime));
			memset(pdx->m_bDecoderBW, 0x0, sizeof(pdx->m_bDecoderBW));
			
			pdx->m_bACapIntDone = 0;
			pdx->m_dwVideoDMAEnable = 0;
			pdx->m_dwAudioDMAEnable = 0;
			
			LocalTimeInit(pdx);

			if(byData & DMA_VIDEO_RECORD)
			{
				for(i=0; i<MAX_DECODER; i++)
				{
					for(j=0; j<MAX_VIDEO_QUEUE; j++)
						pdx->m_pVCAPStatus[i][j]->byLock = MEM_UNLOCK;

					*pdx->m_pVCapCurQCnt[i] = 0;
				}

				pdx->m_bVCapStarted = TRUE;
				for(i=0; i<MAX_DECODER; i++)
				{
					if(pdx->m_bVideoDMA[i] == TRUE)
					{
						pdx->m_dwVideoDMAMask[i]	|= 0x1<<i;
						pdx->m_dwVideoIntMask[i]	|= 0x1<<i*2;
						pdx->m_dwVideoDMAEnable		|= 0x1<<i;
						pdx->m_dwVideoIntEnable		|= 0x1<<i*2;
					}
				}
			}
			if(byData & DMA_AUDIO_RECORD)
			{
				for(j=0; j<MAX_AUDIO_QUEUE; j++)
					pdx->m_pACAPStatus[j]->byLock = MEM_UNLOCK;
				
				*pdx->m_pACapCurQCnt = 0;
				pdx->m_byCurAudioQueueCount = 0;
				
				if(pdx->m_bAudioDMA == TRUE)
				{
					pdx->m_dwAudioDMAEnable |= 1;
					pdx->m_dwAudioIntEnable |= 3;
					pdx->m_bACapStarted = TRUE;
				}
			}

//			Decoder_Init(pdx);
// 			RealTimeMode(pdx, i);

//			KdPrint(("IOCTL_CAPTURE_START::pdx->m_dwVideoIntMask = %X\n", pdx->m_dwVideoIntMask));
			KdPrint(("IOCTL_CAPTURE_START::pdx->m_dwVideoIntEnable = %X, pdx->m_dwVideoDMAEnable = %X\n",
					pdx->m_dwVideoIntEnable, pdx->m_dwVideoDMAEnable));

			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_DMA_ENABLE), pdx->m_dwVideoDMAEnable);		// 0x1D0
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_INT_ENABLE), pdx->m_dwVideoIntEnable);		// 0x1D4

			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_DMA_ENABLE), pdx->m_dwAudioDMAEnable);		// 0x220
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_INT_ENABLE), pdx->m_dwAudioIntEnable);		// 0x224
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + ALL_INT_ENABLE), 1);								// 0x264
			
			break;
			
		case IOCTL_CAPTURE_STOP:
			KdPrint(("IOCTL_CAPTURE_STOP\n"));
			byData = ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[0];	//Channel

			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + ALL_INT_ENABLE), 0);								// 0x264

			if(byData & DMA_VIDEO_RECORD)
			{
				pdx->m_dwVideoDMAEnable = 0;
				pdx->m_dwVideoIntEnable = 0;
				WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_DMA_ENABLE), pdx->m_dwVideoDMAEnable);	
				WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_INT_ENABLE), pdx->m_dwVideoIntEnable);
				pdx->m_bVCapStarted = FALSE;
				WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_INT_STATUS), VCAP_INT_STATUS);
			}
			if(byData & DMA_AUDIO_RECORD)
			{
				pdx->m_dwAudioDMAEnable = 0;
				pdx->m_dwAudioIntEnable = 0;
				WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_DMA_ENABLE), pdx->m_dwAudioDMAEnable);	
				WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_INT_ENABLE), pdx->m_dwAudioIntEnable);
				pdx->m_bACapStarted = FALSE;

				WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_INT_STATUS), ACAP_INT_STATUS);
				pdx->m_dwCapAudioIntStatus = 0;
			}		

			for(i=0; i<MAX_VIDEO_CHANNEL; i++)
				pdx->m_bSync[i] = FALSE;

			break;

		case IOCTL_SET_CHANNEL_TABLE:		
			byRecPath = ((BYTE*)pIrp->AssociatedIrp.SystemBuffer)[MAX_CHANNEL_CHANGEABLE];
			pdx->m_byMaxChannel = ((BYTE*)pIrp->AssociatedIrp.SystemBuffer)[MAX_CHANNEL_CHANGEABLE+1];
			
			memcpy(pdx->m_pbyChannelTable[byRecPath], (BYTE*)pIrp->AssociatedIrp.SystemBuffer, pdx->m_byMaxChannel);

 			KdPrint(("IOCTL_SET_CHANNEL_TABLE::Record_path = [%d], m_byMaxChannelLen = [%d]\n", byRecPath, pdx->m_byMaxChannel));
 			KdPrint(("pdx->m_pbyChannelTable[%d][0] = %X, pdx->m_pbyChannelTable[%d][1] = %X\n", byRecPath, pdx->m_pbyChannelTable[byRecPath][0], byRecPath, pdx->m_pbyChannelTable[byRecPath][1]));
 			KdPrint(("pdx->m_pbyChannelTable[%d][2] = %X, pdx->m_pbyChannelTable[%d][3] = %X\n", byRecPath, pdx->m_pbyChannelTable[byRecPath][2], byRecPath, pdx->m_pbyChannelTable[byRecPath][3]));
			break;

		case IOCTL_SET_COLOR_PARAMETER:
			//KdPrint(("IOCTL_SET_COLOR_PARAMETER\n"));
			byChannel						= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[0];
			pdx->m_byBrightness[byChannel]	= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[1];
			pdx->m_byContrast[byChannel]	= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[2];
			pdx->m_byColor[byChannel]		= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[3];
			pdx->m_byHue[byChannel]			= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[4];

			dwLocalData = READ_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + NVP1204_BRIGHT + 0x10*byChannel));
			dwLocalData &= 0xff00ffff;
			dwLocalData |= (pdx->m_byBrightness[byChannel] << 16);
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + NVP1204_BRIGHT + 0x10*byChannel), dwLocalData);

			dwLocalData = READ_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + NVP1204_CONTRAST + 0x10*byChannel));
			dwLocalData &= 0xffff00ff;
			dwLocalData |= (pdx->m_byContrast[byChannel] << 8);
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + NVP1204_CONTRAST + 0x10*byChannel), dwLocalData);

			dwLocalData = READ_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + NVP1204_COLOR + 0x10*byChannel));
			dwLocalData &= 0x00ffffff;
			dwLocalData |= (pdx->m_byColor[byChannel] << 24);
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + NVP1204_COLOR + 0x10*byChannel), dwLocalData);

			dwLocalData = READ_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + NVP1204_HUE + 0x10*byChannel));
			dwLocalData &= 0xffffff00;
			dwLocalData |= pdx->m_byHue[byChannel];
			WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + NVP1204_HUE + 0x10*byChannel), dwLocalData);

			break;

		case IOCTL_SET_VIDEO_DMA:
			byChannel					= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[0];
			pdx->m_bVideoDMA[byChannel] = ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[1];
			break;

		case IOCTL_SET_AUDIO_DMA:
			pdx->m_bAudioDMA = ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[0];
			break;

		case IOCTL_SET_CAMERA_COLOR_CONTROL:
			byChannel					= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[0];
			pdx->m_bVideoBW[byChannel]	= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[1];
			for(i=0; i<4; i++)
				KdPrint(("IOCTL_SET_COLOR::Channel=%d, VideoBW=%d\n", i+1, pdx->m_bVideoBW[i]));
			break;

		case IOCTL_SET_BRIGHTNESS:
			KdPrint(("IOCTL_SET_BRIGHTNESS\n"));
			byChannel						= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[0];
			pdx->m_byBrightness[byChannel]	= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[1];
			break;
		
		case IOCTL_SET_CONTRAST:
			KdPrint(("IOCTL_SET_CONTRAST\n"));
			byChannel						= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[0];
			pdx->m_byContrast[byChannel]	= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[1];
			break;
		
		case IOCTL_SET_COLOR:
			KdPrint(("IOCTL_SET_COLOR\n"));
			byChannel						= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[0];
			pdx->m_byColor[byChannel]		= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[1];
			break;
		
		case IOCTL_SET_HUE:
			KdPrint(("IOCTL_SET_HUE\n"));
			byChannel						= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[0];
			pdx->m_byHue[byChannel]			= ((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[1];
			break;

		case IOCTL_SET_LOCAL_REGISTER:
			dwLocalAddress	= ((DWORD *)pIrp->AssociatedIrp.SystemBuffer)[0];		
			dwLocalData		= ((DWORD *)pIrp->AssociatedIrp.SystemBuffer)[1];
			KdPrint(("IOCTL_SET_LOCAL_REGISTER::ID[%X]:dwLocalAddress[%X], byData[%X]\n", pdx->m_byDeviceID, dwLocalAddress, dwLocalData));

			WRITE_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + dwLocalAddress), dwLocalData);
			break;

		case IOCTL_GET_LOCAL_REGISTER:
			dwLocalAddress	= ((DWORD *)pIrp->AssociatedIrp.SystemBuffer)[0];
			dwLocalData		= READ_REGISTER_ULONG((DWORD *)(pdx->m_pbyMembase + dwLocalAddress));
			KdPrint(("IOCTL_GET_LOCAL_REGISTER::ID[%X]:dwLocalAddress[%X], byData[%X]\n", pdx->m_byDeviceID, dwLocalAddress, dwLocalData));
			((DWORD*)pIrp->AssociatedIrp.SystemBuffer)[0] = dwLocalData;
			dwRetSize = sizeof(DWORD);
			
//			KdPrint(("pdx->m_pbyMembase + dwLocalAddress = [%X]\n", pdx->m_pbyMembase + dwLocalAddress));
			break;

		case IOCTL_I2C_WRITE_3BYTE:
			wInBuf[0] = ((WORD *)pIrp->AssociatedIrp.SystemBuffer)[0];	// Slave Address
			wInBuf[1] = ((WORD *)pIrp->AssociatedIrp.SystemBuffer)[1];	// Sub Address
			wInBuf[2] = ((WORD *)pIrp->AssociatedIrp.SystemBuffer)[2];	// Data
			wInBuf[3] = ((WORD *)pIrp->AssociatedIrp.SystemBuffer)[3];	// port
			
			I2C_Write(pdx, (BYTE)wInBuf[3], (BYTE)wInBuf[0], (BYTE)wInBuf[1], (BYTE)wInBuf[2]);
			KdPrint(("IOCTL_I2C_WRITE:: Port = %d, DeviceID = %X, reg-address = %X, data = %X\n",
					wInBuf[3], wInBuf[0], wInBuf[1], wInBuf[2]));
			break;

		case IOCTL_I2C_READ_1BYTE:
			wInBuf[0] = ((WORD *)pIrp->AssociatedIrp.SystemBuffer)[0];	// Slave Address
			wInBuf[1] = ((WORD *)pIrp->AssociatedIrp.SystemBuffer)[1];	// Sub Address		
			wInBuf[2] = ((WORD *)pIrp->AssociatedIrp.SystemBuffer)[2];	// I2C port		
			
			I2C_Read(pdx, (BYTE)wInBuf[2], (BYTE)wInBuf[0], (BYTE)wInBuf[1], &byOutBuf[0]);
			((BYTE *)pIrp->AssociatedIrp.SystemBuffer)[0] = byOutBuf[0];

			KdPrint(("IOCTL_I2C_READ::port = %d, DeviceID = %X, Reg-address = %X, data = %X\n",	wInBuf[2], wInBuf[0], wInBuf[1], byOutBuf[0]));
			dwRetSize = sizeof(BYTE);
			break;

		default:
			Status = STATUS_INVALID_DEVICE_REQUEST;
			break;
	}
	IoReleaseRemoveLock(&pdx->m_RemoveLock, pIrp);
	return CompleteRequest(pIrp, Status, dwRetSize);
}