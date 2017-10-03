//
// nothingtest.c
//
// Win32 console mode program to send requests to the
// NOTHING driver.
//
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winioctl.h>
#include <nothing_ioctl.h>
#include <setupapi.h>

#include <initguid.h> // required for GUID definitions
DEFINE_GUID (GUID_DEVINTERFACE_NOTHING,
0x79963ae7, 0x45de, 0x4a31, 0x8f, 0x34, 0xf0, 0xe8, 0x90, 0xb, 0xc2, 0x17);

HANDLE
OpenNothingDeviceViaInterface(
    VOID
    );

//
// Simple test application to demonstrate the Nothing driver
//

int
__cdecl
main(ULONG argc, LPSTR *argv)
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
    memset(readBuffer, 0xAA, sizeof(readBuffer));

    memset(writeBuffer, 0xEE, sizeof(writeBuffer));

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

        printf("Opening Nothing device via its name. Specify -i to open via "\
               "Device Interface\n\n");

        //
        // Open the nothing device by name
        //
        deviceHandle = CreateFile(L"\\\\.\\NOTHING",
                                  GENERIC_READ|GENERIC_WRITE,
                                  0,
                                  0,
                                  OPEN_EXISTING,
                                  0,
                                  0);
    }

    //
    // If this call fails, check to figure out what the error is and report it.
    //
    if (deviceHandle == INVALID_HANDLE_VALUE) {

        code = GetLastError();

        printf("CreateFile failed with error 0x%x\n", code);

        return(code);
    }

    //
    // Infinitely print out the list of choices, ask for input, process
    // the request
    //
    while(TRUE)  {

        printf ("\nNOTHING TEST -- functions:\n\n");
        printf ("\t1. Send a READ\n");
        printf ("\t2. Send a WRITE\n");
        printf ("\t3. Print READ buffer\n");
        printf ("\t4. Print WRITE buffer\n");
        printf ("\t5. Send NOTHING IOCTL\n");
        printf ("\n\t0. Exit\n");
        printf ("\n\tSelection: ");
        scanf ("%x", &function);

        switch(function)  {

        case 1:
            //
            // Send a read
            //
            if ( !ReadFile(deviceHandle,
                           readBuffer,
                           sizeof(readBuffer),
                           &bytesRead,
                           NULL)) {

                code = GetLastError();

                printf("ReadFile failed with error 0x%x\n", code);

                return(code);
            } 

            printf("Read Success! Bytes Read = %d.\n", bytesRead);

            break;

        case 2:
            //
            // Send a write
            //
            if (!WriteFile(deviceHandle,
                           writeBuffer,
                           sizeof(writeBuffer),
                           &bytesRead, 
                           NULL)) {

                code = GetLastError();

                printf("WriteFile failed with error 0x%x\n", code);

                return(code);
            } 

            printf("Write Success! Bytes Written = %d.\n", bytesRead);

            break;

        case 3:
            //
            // Show read buffer
            //
            printf("Printing first 32 bytes of READ buffer\n");

            for(index = 0; index < 32; index++)  {

                printf("0x%0x,",readBuffer[index]);
            }

            break;

        case 4:
            //
            // Show WRITE buffer
            //
            printf("Printing first 32 bytes of WRITE buffer\n");

            for(index = 0; index < 32; index++)  {

                printf("0x%0x,",writeBuffer[index]);
            }

            break;

        case 5:
            //
            // Test the IOCTL interface
            //
            if (!DeviceIoControl(deviceHandle,
                                 (DWORD)IOCTL_OSR_NOTHING,
                                 0,
                                 0,
                                 0,
                                 0,
                                 &bytesRead,
                                 NULL)) {

                code = GetLastError();

                printf("DeviceIoControl failed with error 0x%x\n", code);

                return(code);
            } 

            printf("DeviceIoControl Success! Bytes Read = %d.\n", bytesRead);

            break;

        case 0:

            //
            // zero is get out!
            //
            return(0);

        }
    }   
}


HANDLE
OpenNothingDeviceViaInterface(
    VOID
    ) {

    HDEVINFO                         devInfo;
    SP_DEVICE_INTERFACE_DATA         devInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA devInterfaceDetailData = NULL;
    ULONG                            devIndex;
    ULONG                            requiredSize;
    ULONG                            code;
    HANDLE                           handle;

    //
    // Open a handle to the device using the 
    //  device interface that the driver registers
    //
    //

    //
    // Get the device information set for all of the
    //  devices of our class (the GUID we defined
    //  in nothingioctl.h and registered in the driver
    //  with DfwDeviceCreateDeviceInterface) that are present in the 
    //  system
    //
    devInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_NOTHING,
                                  NULL,
                                  NULL,
                                  DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (devInfo == INVALID_HANDLE_VALUE) {

        printf("SetupDiGetClassDevs failed with error 0x%x\n", GetLastError());

        return INVALID_HANDLE_VALUE;

    }

    //
    // Now get information about each device installed...
    //
    
    //
    // This needs to be set before calling
    //  SetupDiEnumDeviceInterfaces
    //
    devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    //
    // Start with the first device...
    //
    devIndex = 0;

    while (SetupDiEnumDeviceInterfaces(devInfo,
                                       NULL,
                                       &GUID_DEVINTERFACE_NOTHING,
                                       devIndex++, 
                                       &devInterfaceData)) {

        //
        // If you actually had a reason to keep
        //  track of all the devices in the system
        //  you obviously wouldn't want to just
        //  throw these away. Since we're just 
        //  running through to print them out
        //  and picking whatever the last one
        //  is we'll alloc and free these
        //  as we go...
        //
        if (devInterfaceDetailData != NULL) {

            free(devInterfaceDetailData);

            devInterfaceDetailData = NULL;

        }

        //
        // The entire point of this exercise is
        //  to get a string that we can hand to 
        //  CreateFile to get a handle to the device,
        //  so we need to call SetupDiGetDeviceInterfaceDetail
        //  (which will give us the string we need)
        //

        //
        // First call it with a NULL output buffer to
        //  get the number of bytes needed...
        //
        if (!SetupDiGetDeviceInterfaceDetail(devInfo,
                                             &devInterfaceData,
                                             NULL,
                                             0,
                                             &requiredSize,
                                             NULL)) {

            code = GetLastError();

            //
            // We're expecting ERROR_INSUFFICIENT_BUFFER.
            //  If we get anything else there's something
            //  wrong...
            //
            if (code != ERROR_INSUFFICIENT_BUFFER) {

                printf("SetupDiGetDeviceInterfaceDetail failed with error 0x%x\n", code);

                //
                // Clean up the mess...
                //
                SetupDiDestroyDeviceInfoList(devInfo);
                
                return INVALID_HANDLE_VALUE;

            }

        }

        //
        // Allocated a PSP_DEVICE_INTERFACE_DETAIL_DATA...
        //
        // requiredSize is set by SetupDiGetDeviceInterfaceDetail on failure,
        // surpress the CA warning that doesn't understand this
        //
#pragma warning(suppress: 6102)
        devInterfaceDetailData = 
                (PSP_DEVICE_INTERFACE_DETAIL_DATA) malloc(requiredSize);

        if (!devInterfaceDetailData) {

            printf("Unable to allocate resources...Exiting\n");

            //
            // Clean up the mess...
            //
            SetupDiDestroyDeviceInfoList(devInfo);

            return INVALID_HANDLE_VALUE;

        }

        //
        // This needs to be set before calling
        //  SetupDiGetDeviceInterfaceDetail. You
        //  would *think* that you should be setting
        //  cbSize to requiredSize, but that's not the 
        //  case. 
        //
        devInterfaceDetailData->cbSize = 
                    sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);


        if (!SetupDiGetDeviceInterfaceDetail(devInfo,
                                             &devInterfaceData,
                                             devInterfaceDetailData,
                                             requiredSize,
                                             &requiredSize,
                                             NULL)) {

            printf("SetupDiGetDeviceInterfaceDetail failed with error 0x%x\n", GetLastError());

            //
            // Clean up the mess...
            //
            SetupDiDestroyDeviceInfoList(devInfo);

            free(devInterfaceDetailData);

            return INVALID_HANDLE_VALUE;

        }

        //
        // Got one!
        //
        printf("Device found! %ls\n", devInterfaceDetailData->DevicePath);


    }

    code = GetLastError();

    //
    // We shouldn't get here until SetupDiGetDeviceInterfaceDetail
    //  runs out of devices to enumerate
    //
    if (code != ERROR_NO_MORE_ITEMS) {

        printf("SetupDiGetDeviceInterfaceDetail failed with error 0x%x\n", code);
        
        //
        // Clean up the mess...
        //
        SetupDiDestroyDeviceInfoList(devInfo);
        
        free(devInterfaceDetailData);
        
        return INVALID_HANDLE_VALUE;

    }

    SetupDiDestroyDeviceInfoList (devInfo);
    
    if(devInterfaceDetailData == NULL) {
        
        printf("Unable to find any Nothing devices!\n");

        return INVALID_HANDLE_VALUE;

    }

    //
    // We don't really care which one we get, 
    //  just open whatever happened to be the 
    //  last one...
    //
    printf("Opening device interface %ls\n", 
                    devInterfaceDetailData->DevicePath);
    

    handle = CreateFile(devInterfaceDetailData->DevicePath,
                        GENERIC_READ|GENERIC_WRITE,
                        0,
                        0,
                        OPEN_EXISTING,
                        0,
                        0);

    free(devInterfaceDetailData);

    return handle;

}

