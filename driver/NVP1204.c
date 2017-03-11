
#include "NVP1204.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, AL_AddDevice)
#pragma alloc_text (PAGE, AL_DispatchPnp)
#pragma alloc_text (PAGE, AL_DispatchControl)
#pragma alloc_text (PAGE, AL_DispatchCreate)
#pragma alloc_text (PAGE, AL_DispatchClose)
#pragma alloc_text (PAGE, AL_DispatchWrite)
#pragma alloc_text (PAGE, AL_DispatchPower)
#pragma alloc_text (PAGE, AL_Unload)
#pragma alloc_text (PAGE, AL_StartDevice)
#endif

LONG	g_DevCount = -1;
LONG	g_dwDeviceCount = -1;
//=======================================================================//
NTSTATUS DriverEntry(IN PDRIVER_OBJECT  DriverObject, IN PUNICODE_STRING RegistryPath)
{	
	UNREFERENCED_PARAMETER(RegistryPath);

	KdPrint(("Entered Driver Entry\n"));

	// Create dispatch points for the IRPs.
	DriverObject->MajorFunction[IRP_MJ_CREATE]         = AL_DispatchCreate;//Create
	DriverObject->MajorFunction[IRP_MJ_CLOSE]          = AL_DispatchClose;
	DriverObject->MajorFunction[IRP_MJ_WRITE]			= AL_DispatchWrite;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = AL_DispatchControl;
	DriverObject->DriverUnload                         = AL_Unload;
	DriverObject->MajorFunction[IRP_MJ_PNP]            = AL_DispatchPnp;
	DriverObject->MajorFunction[IRP_MJ_POWER]          = AL_DispatchPower;
	DriverObject->DriverExtension->AddDevice           = AL_AddDevice;

	return STATUS_SUCCESS;
}
//=======================================================================//
NTSTATUS AL_AddDevice(IN PDRIVER_OBJECT DriverObject, IN PDEVICE_OBJECT PhysicalDeviceObject)
{
	NTSTATUS			status = STATUS_SUCCESS;
	PDEVICE_OBJECT		deviceObject = NULL;
	PDEVICE_EXTENSION	pdx;
	UNICODE_STRING		ntDeviceName;
	WCHAR				DeviceNameBuf[0x100], win32NameBuf[0x100];
	
	memset(DeviceNameBuf, 0, 0x100*2);
	memset(win32NameBuf, 0, 0x100*2);
	
	g_dwDeviceCount = InterlockedIncrement(&g_DevCount);
	KdPrint(("AddDevice::g_dwDeviceCount[%d]:g_DevCount[%d]\n", g_dwDeviceCount, g_DevCount));
	_snwprintf(DeviceNameBuf, arraysize(DeviceNameBuf), NT_DEVICE_NAME, g_dwDeviceCount);
	RtlInitUnicodeString(&ntDeviceName, DeviceNameBuf);
	
	status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), &ntDeviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &deviceObject);
	if(!NT_SUCCESS (status))
	{
		g_dwDeviceCount = InterlockedDecrement(&g_DevCount);
		KdPrint(("IoCreateDevice fail::g_dwDeviceCount==%d g_DevCount == %d \n", g_dwDeviceCount, g_DevCount));
		return status;
	}
	
	pdx = (PDEVICE_EXTENSION) deviceObject->DeviceExtension;
	_snwprintf(win32NameBuf, 0x100, DOS_DEVICE_NAME, g_dwDeviceCount);
	RtlInitUnicodeString(&pdx->m_win32DeviceName, win32NameBuf);
	status = IoCreateSymbolicLink( &pdx->m_win32DeviceName, &ntDeviceName );
	
	if (!NT_SUCCESS(status))    // If we we couldn't create the link then
	{                           //  abort installation.
		KdPrint(("IoCreateSymbolicLink fail::g_dwDeviceCount==%d g_DevCount == %d \n", g_dwDeviceCount, g_DevCount));
		g_dwDeviceCount = InterlockedDecrement(&g_DevCount);
		IoDeleteDevice(deviceObject);
		return status;
	}
   	
	pdx->m_pLowerDeviceObject = IoAttachDeviceToDeviceStack(deviceObject, PhysicalDeviceObject);
	if(NULL == pdx->m_pLowerDeviceObject) 
	{
		KdPrint(("IoAttachDeviceToDeviceStack fail::g_dwDeviceCount==%d g_DevCount == %d \n", g_dwDeviceCount, g_DevCount));
		g_dwDeviceCount = InterlockedDecrement(&g_DevCount);
		IoDeleteSymbolicLink(&pdx->m_win32DeviceName);
		IoDeleteDevice(deviceObject);
		return STATUS_NO_SUCH_DEVICE;
	}

	deviceObject->Flags |=  DO_POWER_PAGABLE | DO_DIRECT_IO;
	deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
	
	pdx->m_pPdo	 = PhysicalDeviceObject;
	pdx->m_bRemoved = FALSE;
	pdx->m_bStarted = FALSE;

	pdx->m_byDeviceID		= (BYTE)g_dwDeviceCount;
	pdx->m_pInterruptObject	= NULL;
	pdx->m_bBufferAllocate	= FALSE;
	pdx->m_bMDLAllocate		= FALSE;
	
	IoInitializeRemoveLock(&pdx->m_RemoveLock, 0, 0, 0);

	KeInitializeDpc(&pdx->m_DpcObjectVideo,(PKDEFERRED_ROUTINE)DpcForIsr_Video, pdx);	
	KeInitializeDpc(&pdx->m_DpcObjectAudio,(PKDEFERRED_ROUTINE)DpcForIsr_Audio, pdx);	
	return STATUS_SUCCESS;	
}
//=======================================================================//
NTSTATUS AL_CompletionRoutine(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp, IN PVOID Context)
{
	PKEVENT     event;
	
	event = (PKEVENT)Context;	
	UNREFERENCED_PARAMETER(DeviceObject);
	if(Irp->PendingReturned) 
		IoMarkIrpPending(Irp);
	KeSetEvent(event, 0, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;
}
//=======================================================================//
VOID AL_Unload(IN PDRIVER_OBJECT DriverObject)
{
	ASSERT(DriverObject->DeviceObject == NULL);	
	KdPrint(("AL_Unload\n"));	
	return;
}
//=======================================================================//
#if DBG
PCHAR PnPMinorFunctionString(BYTE MinorFunction)
{
	switch(MinorFunction)
	{
	case IRP_MN_START_DEVICE:
		return "IRP_MN_START_DEVICE";
	case IRP_MN_QUERY_REMOVE_DEVICE:
		return "IRP_MN_QUERY_REMOVE_DEVICE";
	case IRP_MN_REMOVE_DEVICE:
		return "IRP_MN_REMOVE_DEVICE";
	case IRP_MN_CANCEL_REMOVE_DEVICE:
		return "IRP_MN_CANCEL_REMOVE_DEVICE";
	case IRP_MN_STOP_DEVICE:
		return "IRP_MN_STOP_DEVICE";
	case IRP_MN_QUERY_STOP_DEVICE:
		return "IRP_MN_QUERY_STOP_DEVICE";
	case IRP_MN_CANCEL_STOP_DEVICE:
		return "IRP_MN_CANCEL_STOP_DEVICE";
	case IRP_MN_QUERY_DEVICE_RELATIONS:
		return "IRP_MN_QUERY_DEVICE_RELATIONS";
	case IRP_MN_QUERY_INTERFACE:
		return "IRP_MN_QUERY_INTERFACE";
	case IRP_MN_QUERY_CAPABILITIES:
		return "IRP_MN_QUERY_CAPABILITIES";
	case IRP_MN_QUERY_RESOURCES:
		return "IRP_MN_QUERY_RESOURCES";
	case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
		return "IRP_MN_QUERY_RESOURCE_REQUIREMENTS";
	case IRP_MN_QUERY_DEVICE_TEXT:
		return "IRP_MN_QUERY_DEVICE_TEXT";
	case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:
		return "IRP_MN_FILTER_RESOURCE_REQUIREMENTS";
	case IRP_MN_READ_CONFIG:
		return "IRP_MN_READ_CONFIG";
	case IRP_MN_WRITE_CONFIG:
		return "IRP_MN_WRITE_CONFIG";
	case IRP_MN_EJECT:
		return "IRP_MN_EJECT";
	case IRP_MN_SET_LOCK:
		return "IRP_MN_SET_LOCK";
	case IRP_MN_QUERY_ID:
		return "IRP_MN_QUERY_ID";
	case IRP_MN_QUERY_PNP_DEVICE_STATE:
		return "IRP_MN_QUERY_PNP_DEVICE_STATE";
	case IRP_MN_QUERY_BUS_INFORMATION:
		return "IRP_MN_QUERY_BUS_INFORMATION";
	case IRP_MN_DEVICE_USAGE_NOTIFICATION:
		return "IRP_MN_DEVICE_USAGE_NOTIFICATION";
	case IRP_MN_SURPRISE_REMOVAL:
		return "IRP_MN_SURPRISE_REMOVAL";
		
	default:
		return "IRP_MN_?????";
	}
}
#endif

//=========================================================================================================================//
NTSTATUS CompleteRequest(IN PIRP pIrp, IN NTSTATUS status, IN ULONG_PTR info)
{							// CompleteRequest
	pIrp->IoStatus.Status		= status;
	pIrp->IoStatus.Information = info;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return status;
}	
