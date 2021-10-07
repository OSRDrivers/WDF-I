//
// NOTHINGTEST.CPP
//
// Test utility for the OSR Nothing driver, which was created for our WDF seminar.
//
// This code is purely functional, and is definitely not designed to be any
// sort of example.
//
#define _CRT_SECURE_NO_WARNINGS

#include <cstdio>
#include <Windows.h>
#include <cfgmgr32.h>
#include <nothing_ioctl.h>

HANDLE
OpenNothingDeviceViaInterface(
    VOID);

//
// Simple test application to demonstrate the Nothing driver
//

int
__cdecl
main(ULONG  argc,
     LPSTR* argv)
{
    HANDLE deviceHandle;
    DWORD  code;
    UCHAR  writeBuffer[4096];
    UCHAR  readBuffer[9096];
    DWORD  bytesRead;
    DWORD  index;
    DWORD  function;

    UNREFERENCED_PARAMETER(argv);

    //
    // Init the write and read buffers with known data
    //
    memset(readBuffer,
           0xAA,
           sizeof(readBuffer));

    memset(writeBuffer,
           0xEE,
           sizeof(writeBuffer));

    //
    // Check to see if the user wants to open the device via the
    // interface
    //
    if (argc == 2) {

        //
        // An argument specified (we're lazy, we don't care what it is)
        // try to open via the interface
        //
        deviceHandle = OpenNothingDeviceViaInterface();

    } else {

        printf("Opening Nothing device via its name. Specify -i to open via "
               "Device Interface\n\n");

        //
        // Open the nothing device by name
        //
        deviceHandle = CreateFile(L"\\\\.\\NOTHING",
                                  GENERIC_READ | GENERIC_WRITE,
                                  0,
                                  nullptr,
                                  OPEN_EXISTING,
                                  0,
                                  nullptr);
    }

    //
    // If this call fails, check to figure out what the error is and report it.
    //
    if (deviceHandle == INVALID_HANDLE_VALUE) {

        code = GetLastError();

        printf("CreateFile failed with error 0x%lx\n",
               code);

        return (code);
    }

    //
    // Infinitely print out the list of choices, ask for input, process
    // the request
    //
    while (TRUE) {

        printf("\nNOTHING TEST -- functions:\n\n");
        printf("\t1. Send a READ\n");
        printf("\t2. Send a WRITE\n");
        printf("\t3. Print READ buffer\n");
        printf("\t4. Print WRITE buffer\n");
        printf("\t5. Send NOTHING IOCTL\n");
        printf("\n\t0. Exit\n");
        printf("\n\tSelection: ");

#pragma warning(suppress: 6031)
        scanf("%x",
              &function);   // NOLINT(cert-err34-c)

        switch (function) {

            case 1:
                //
                // Send a read
                //
                if (!ReadFile(deviceHandle,
                              readBuffer,
                              sizeof(readBuffer),
                              &bytesRead,
                              nullptr)) {

                    code = GetLastError();

                    printf("ReadFile failed with error 0x%lx\n",
                           code);

                    return (code);
                }

                printf("Read Success! Bytes Read = %lu.\n",
                       bytesRead);

                break;

            case 2:
                //
                // Send a write
                //
                if (!WriteFile(deviceHandle,
                               writeBuffer,
                               sizeof(writeBuffer),
                               &bytesRead,
                               nullptr)) {

                    code = GetLastError();

                    printf("WriteFile failed with error 0x%lx\n",
                           code);

                    return (code);
                }

                printf("Write Success! Bytes Written = %lu.\n",
                       bytesRead);

                break;

            case 3:
                //
                // Show read buffer
                //
                printf("Printing first 32 bytes of READ buffer\n");

                for (index = 0; index < 32; index++) {

                    printf("0x%0x,",
                           readBuffer[index]);
                }

                break;

            case 4:
                //
                // Show WRITE buffer
                //
                printf("Printing first 32 bytes of WRITE buffer\n");

                for (index = 0; index < 32; index++) {

                    printf("0x%0x,",
                           writeBuffer[index]);
                }

                break;

            case 5:
                //
                // Test the IOCTL interface
                //
                if (!DeviceIoControl(deviceHandle,
                                     (DWORD)IOCTL_OSR_NOTHING,
                                     nullptr,
                                     0,
                                     nullptr,
                                     0,
                                     &bytesRead,
                                     nullptr)) {

                    code = GetLastError();

                    printf("DeviceIoControl failed with error 0x%lx\n",
                           code);

                    return (code);
                }

                printf("DeviceIoControl Success! Bytes Read = %lu.\n",
                       bytesRead);

                break;

            case 0:

                //
                // zero is get out!
                //
                return (0);

            default:
                break;

        }
    }
}


HANDLE
OpenNothingDeviceViaInterface()
{
    CONFIGRET configReturn;
    DWORD     lasterror;
    WCHAR     deviceName[MAX_DEVICE_ID_LEN];
    HANDLE    handleToReturn = INVALID_HANDLE_VALUE;

    //
    // Get the device interface -- we only expose one
    //
    deviceName[0] = UNICODE_NULL;

    configReturn = CM_Get_Device_Interface_List((LPGUID)&GUID_DEVINTERFACE_NOTHING,
                                                nullptr,
                                                deviceName,
                                                _countof(deviceName),
                                                CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (configReturn != CR_SUCCESS) {

        lasterror = GetLastError();

        printf("CM_Get_Device_Interface_List fail: %lx\n",
               lasterror);

        goto Exit;
    }

    //
    // Make sure there's an actual name there
    //
    if (deviceName[0] == UNICODE_NULL) {
        lasterror = ERROR_NOT_FOUND;
        printf("CreateFile fail: %lx\n",
               lasterror);
        goto Exit;
    }

    //
    // Open the device
    //
    handleToReturn = CreateFile(deviceName,
                                GENERIC_WRITE | GENERIC_READ,
                                0,
                                nullptr,
                                OPEN_EXISTING,
                                0,
                                nullptr);

    if (handleToReturn == INVALID_HANDLE_VALUE) {
        lasterror = GetLastError();
        printf("CreateFile fail: %lx\n",
               lasterror);
    }

Exit:

    //
    // Return a handle to the device
    //
    return handleToReturn;
}
