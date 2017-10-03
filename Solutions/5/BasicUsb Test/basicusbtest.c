//
// basicusbtest.c
//
// Win32 console mode program to fire up requests to the
// BASICUSB driver.
//
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <windows.h>
#include <winioctl.h>
#include <basicusb_ioctl.h>

//
// Simple test application to demonstrate the BasicUSB driver
//

__cdecl main(ULONG argc, LPSTR *argv)
{
    HANDLE          DriverHandle;
    DWORD           code;
    ULONG           index;
    UCHAR           WriteBuffer[256];
    UCHAR           ReadBuffer[256];
    DWORD           Function;
    UCHAR           barGraph;
    UCHAR           switchPackState;

    //
    // Init the write and read buffers with known data
    //
    memset(ReadBuffer, 0xAA, sizeof(ReadBuffer));

    memset(WriteBuffer, 0xEE, sizeof(WriteBuffer));

    //
    // Open the sample PCI device
    //
    DriverHandle = CreateFile(L"\\\\.\\BASICUSB",  // Name of the NT "device" to open
                            GENERIC_READ|GENERIC_WRITE,  // Access rights requested
                            0,                           // Share access - NONE
                            0,                           // Security attributes - not used!
                            OPEN_EXISTING,               // Device must exist to open it.
                            0,                           // Open for sync I/O
                            0);                          // extended attributes - not used!

    //
    // If this call fails, check to figure out what the error is and report it.
    //
    if (DriverHandle == INVALID_HANDLE_VALUE) {

        code = GetLastError();

        printf("CreateFile failed with error 0x%x\n", code);

        return(code);
    }

    //
    // Infinitely print out the list of choices, ask for input, process
    // the request
    //
    while(TRUE)  {

        printf ("\nBASICUSB TEST -- Functions:\n\n");
        printf ("\t1. Send a sync READ\n");
        printf ("\t2. Send a sync WRITE\n");
        printf ("\t3. Send SET_BAR_GRAPH IOCTL - All On\n");
        printf ("\t4. Send SET_BAR_GRAPH IOCTL - All Off\n");
        printf ("\t5. Get switch pack state\n");
        printf ("\n\t0. Exit\n");
        printf ("\n\tSelection: ");
        scanf ("%x", &Function);

        switch(Function)  {

        case 1:
            //
            // Send a read
            //
            if ( !ReadFile(DriverHandle,
                            ReadBuffer,
                            sizeof(ReadBuffer),
                            &index,
                            NULL)) {

                code = GetLastError();

                printf("ReadFile failed with error 0x%x\n", code);

                return(code);
            }

            printf("Bytes Read = %d.\n", index);
            break;

        case 2:
            //
            // Send a write
            //
            if (!WriteFile(DriverHandle,
                            WriteBuffer,
                            sizeof(WriteBuffer),
                            &index, 
                            NULL)) {

                code = GetLastError();

                printf("WriteFile failed with error 0x%x\n", code);

                return(code);
            }
            printf("Bytes Written = %d.\n", index);
            break;

        case 3:
            barGraph = 0xFF;

            if (!DeviceIoControl(DriverHandle,
                                IOCTL_OSR_BASICUSB_SET_BAR_GRAPH,
                                &barGraph,      // Ptr to InBuffer
                                sizeof(UCHAR), // Length of InBuffer
                                NULL,     // Ptr to OutBuffer
                                0, // Length of OutBuffer
                                &index,        // BytesReturned
                                NULL)) {

                code = GetLastError();

                printf("DeviceIoControl failed with error 0x%x\n", code);
                return(code);

            }

            printf("IOCTL worked!\n");
            break;
        case 4:
            barGraph = 0;

            if (!DeviceIoControl(DriverHandle,
                                IOCTL_OSR_BASICUSB_SET_BAR_GRAPH,
                                &barGraph,      // Ptr to InBuffer
                                sizeof(UCHAR), // Length of InBuffer
                                NULL,     // Ptr to OutBuffer
                                0, // Length of OutBuffer
                                &index,        // BytesReturned
                                NULL)) {

                code = GetLastError();

                printf("DeviceIoControl failed with error 0x%x\n", code);
                return(code);

            }
            printf("IOCTL worked!\n");
            break;

        case 5:
            switchPackState = 0;

            if (!DeviceIoControl(
                            DriverHandle,
                            IOCTL_OSR_BASICUSB_GET_SWITCHPACK_STATE,
                            NULL,      // Ptr to InBuffer
                            0, // Length of InBuffer
                            &switchPackState,     // Ptr to OutBuffer
                            sizeof(UCHAR), // Length of OutBuffer
                            &index,        // BytesReturned
                            NULL)) {

                code = GetLastError();

                printf("DeviceIoControl failed with error 0x%x\n", code);
                return(code);

            }
            printf("IOCTL worked! Switchpack state is 0x%x\n", switchPackState);
            break;
        case 0:

            //
            // zero is get out!
            //
            return(0);

        }
    }   
}
