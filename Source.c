#include <ntddk.h>
//#include <wdf.h>


//� ���� ������� ������� �� ������������ ����� ��������� #pragma alloc_text(INIT, DriverEntry) � #pragma alloc_text(PAGE, UnloadRoutine). ������� ��� ��� ��������: ������ �������� ������� DriverEntry � INIT ������, �� ���� ��� �� �������, ��� DriverEntry ����� ��������� ���� ��� � ����� ����� ��� ������� ����� �������� ��������� �� ������. ������ �������� ��� ������� UnloadRoutine ��� �����������, �.�. ��� �������������, ������� ����� ����������� ��� � ���� ��������, � ����� ������� ��� ������.

/*
VOID Unload(IN PDRIVER_OBJECT pDriverObject);
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath);

#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, Unload)
*/

/*
typedef struct _DEVICE_EXTENSION { PDEVICE_OBJECT pKeyboardDevice;
									PETHREAD pThreadObj;
									bool bhThreadTerminate;
									HANDLE hLogFile;
									KEY_STATE kState;
									KSEMAPHORE semQueue;
									KSPIN_LOCK lockQueue;
									LIST_ENTRY QueueListHead;
									} DEVICE_EXTENSION, *PDEVICE_EXTENSION;
*/

typedef struct _KEYBOARD_INPUT_DATA { USHORT UnitId;
										USHORT MakeCode;
										USHORT Flags;
										USHORT Reserved;
										ULONG ExtraInformation;
										} KEYBOARD_INPUT_DATA, *PKEYBOARD_INPUT_DATA;


typedef struct _DEVICE_EXTENSION {
	PDEVICE_OBJECT pKeyboardDevice;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

LONG64 volatile REQUESTS; //����� ������������� ��������

NTSTATUS ReadCompleted(PDEVICE_OBJECT pDeviceObject, PIRP pIrp, PVOID Context)
{
	//PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pDeviceObject->DeviceExtension;

	PKEYBOARD_INPUT_DATA keys;

	if (pIrp->IoStatus.Status == STATUS_SUCCESS)
	{
		keys = (PKEYBOARD_INPUT_DATA)pIrp->AssociatedIrp.SystemBuffer;
		int nKeys = pIrp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA);

		for (int i = 0; i < nKeys; ++i)
		{
			DbgPrint("Code: %x\n", keys[i].MakeCode);
		}
	}

	if (pIrp->PendingReturned)
	{
		IoMarkIrpPending(pIrp);
		InterlockedDecrement64(&REQUESTS);
	}
		
	return pIrp->IoStatus.Status;

}

NTSTATUS DispatchSkip(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	DbgPrint("Entering dispatchskip\n");
	//���������� IRP ����� ���� �� �����, �� ������� ���
	IoSkipCurrentIrpStackLocation(pIrp);
	return IoCallDriver(((PDEVICE_EXTENSION)pDeviceObject->DeviceExtension)->pKeyboardDevice, pIrp);
}

NTSTATUS DispatchRead(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	InterlockedIncrement64(&REQUESTS);

	//����������� ��������� ����� ��� ������������ ��������
	IoCopyCurrentIrpStackLocationToNext(pIrp);

	//�� ���������� ������� �������� ��������� �������
	IoSetCompletionRoutine(pIrp, ReadCompleted, pDeviceObject, TRUE, TRUE, TRUE);

	//������� IRP ���������� ��������
	return IoCallDriver(((PDEVICE_EXTENSION)pDeviceObject->DeviceExtension)->pKeyboardDevice, pIrp);

}

VOID Unload(PDRIVER_OBJECT pDriverObject)
{
	DbgPrint("entering unload\n");
	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pDriverObject->DeviceObject->DeviceExtension;

	IoDetachDevice(pKeyboardDeviceExtension->pKeyboardDevice);
	
	if (REQUESTS != 0)
	{
		DbgPrint("Waiting timer\n");
		KTIMER ktTimer;
		LARGE_INTEGER liTimeout;
		liTimeout.QuadPart = 1000000;
		KeInitializeTimer(&ktTimer);

		while (REQUESTS > 0)
		{
			KeSetTimer(&ktTimer, liTimeout, NULL);
			KeWaitForSingleObject(&ktTimer, Executive, KernelMode, FALSE, NULL);
		}
		DbgPrint("Delete device\n");
	}
	
	IoDeleteDevice(pDriverObject->DeviceObject);
	return;
}

NTSTATUS KeyboardFilter(PDRIVER_OBJECT pDriverObject)
{
	//DbgPrint("entering kbd filted\n");
	//������ ������� ����������
	PDEVICE_OBJECT pKeyboardDeviceObject;
	NTSTATUS NtStatus = 0;

	//������ ������ �������� (Device object) 
	NtStatus = IoCreateDevice(pDriverObject,
		sizeof(DEVICE_EXTENSION),
		NULL,
		FILE_DEVICE_KEYBOARD,
		0,
		TRUE, //FALSE
		&pKeyboardDeviceObject); //&

	if (NtStatus == STATUS_SUCCESS)
	{
		pKeyboardDeviceObject->Flags |= (DO_BUFFERED_IO | DO_POWER_PAGABLE);
		pKeyboardDeviceObject->Flags &= (~DO_DEVICE_INITIALIZING);

		RtlZeroMemory(pKeyboardDeviceObject->DeviceExtension, sizeof(DEVICE_EXTENSION));
		PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pKeyboardDeviceObject->DeviceExtension;

		CCHAR cName[40] = "\\Device\\KeyboardClass0";
		STRING strName;
		UNICODE_STRING ustrDeviceName;

		RtlInitAnsiString(&strName, cName);
		RtlAnsiStringToUnicodeString(&ustrDeviceName, &strName, TRUE);

		IoAttachDevice(pKeyboardDeviceObject, &ustrDeviceName, &pKeyboardDeviceExtension->pKeyboardDevice);
		RtlFreeUnicodeString(&ustrDeviceName);		
		DbgPrint("Keyboard installed");
	}
	else
	{
		DbgPrint("Installing keyboard filter error");
	}

	return NtStatus;
}


//������� �����
//PDRIVER_OBJECT - ����� ������� ��������
//PUNICODE_STRING - ���� � ������� � ���������� ��������
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath)
{
	//DbgPrint("driver loaded\n");
	REQUESTS = 0;
	NTSTATUS NtStatus = 0; //STATUS_SUCCESS;
	unsigned int uiIndex = 0;

	//DbgPrint("Entry points...\n");

	//����������� ��������� ����� ����� � ���� ������� ���������
	for (uiIndex = 0; uiIndex < IRP_MJ_MAXIMUM_FUNCTION; ++uiIndex)
	{
		//���������� IRP ����� ����
		pDriverObject->MajorFunction[uiIndex] = DispatchSkip;
	}
	//������������� IRP_MJ_READ
	pDriverObject->MajorFunction[IRP_MJ_READ] = DispatchRead;

	//DbgPrint("Entering hook routine\n");
	//�������� ���� ���������� � ����
	NtStatus = KeyboardFilter(pDriverObject);

	//������� ��������
	pDriverObject->DriverUnload = Unload;

	return NtStatus;
}

