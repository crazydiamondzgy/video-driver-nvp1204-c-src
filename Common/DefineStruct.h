#ifndef _DEFINE_STRUCT_H_
#define _DEFINE_STRUCT_H_

#ifndef DEVICE_DRIVER
#include "mmsystem.h"
#else
typedef struct tagFILETIME
{
	DWORD	dwLowDateTime;
	DWORD	dwHighDateTime;
} FILETIME;
#endif

#pragma pack(1)

typedef struct TAG_VCAP_STATUS_INFO
{
	BYTE		byLock;
	BYTE		byChannel;
	BYTE		bySize;
	BYTE		byField;
	FILETIME	VCAPTime;
}VCAP_STATUS_INFO, *PVCAP_STATUS_INFO;

typedef struct TAG_ACAP_STATUS_INFO
{
	BYTE		byLock;
//	FILETIME	ACAPTime;
}ACAP_STATUS_INFO, *PACAP_STATUS_INFO;

#ifndef DEVICE_DRIVER
typedef struct TAG_DISPLAY_INFO
{
	BYTE		byMaxCoodiCount;
	BYTE		byCoodiIndex[MAX_VIDEO_CHANNEL];
	BYTE		byArrayIndex[MAX_VIDEO_CHANNEL];
	RECT		NormalRect[MAX_VIDEO_CHANNEL];
}DISPLAY_INFO, *PDISPLAY_INFO;

typedef struct TAG_DRIVE_INFO
{
	char			szDrive;
	int				nDiskPercent;
	ULARGE_INTEGER	uTotal;
	ULARGE_INTEGER	uFree;	
}DRIVE_INFO, *PDRIVE_INFO;


#endif
#pragma pack( )

#endif
