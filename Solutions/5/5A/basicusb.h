//
// Copyright 2007-2017 OSR Open Systems Resources, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
// CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
// POSSIBILITY OF SUCH DAMAGE
// 

extern "C" {
#include <ntddk.h>
#include <wdf.h>
#include <usbdi.h>
#include <wdfusb.h>

DRIVER_INITIALIZE DriverEntry;

}

//
// Generate our device interface GUID
//
#include <initguid.h> // required for GUID definitions
DEFINE_GUID (GUID_DEVINTERFACE_BASICUSB,
0x79963ae7, 0x45de, 0x4a31, 0x8f, 0x34, 0xf0, 0xe8, 0x90, 0xb, 0xc2, 0x18);

#include "BASICUSB_IOCTL.h"

//
// BasicUsb device context structure
//
// KMDF will associate this structure with each "BasicUsb" device that
// this driver creates.
//
typedef struct _BASICUSB_DEVICE_CONTEXT  {
    //
    // Our USB I/O target
    //
    WDFUSBDEVICE BasicUsbUsbDevice;

    //
    // Our bulk IN pipe
    //
    WDFUSBPIPE   BulkInPipe;

    //
    // Our bulk OUT pipe
    //
    WDFUSBPIPE   BulkOutPipe;

    //
    // Our interrupt IN pipe
    //
    WDFUSBPIPE   InterruptInPipe;
} BASICUSB_DEVICE_CONTEXT, * PBASICUSB_DEVICE_CONTEXT;


//
// The various vendor commands for our device.
//
#define USBFX2LK_READ_7SEGMENT_DISPLAY      0xD4
#define USBFX2LK_READ_SWITCHES              0xD6
#define USBFX2LK_READ_BARGRAPH_DISPLAY      0xD7
#define USBFX2LK_SET_BARGRAPH_DISPLAY       0xD8
#define USBFX2LK_IS_HIGH_SPEED              0xD9
#define USBFX2LK_REENUMERATE                0xDA
#define USBFX2LK_SET_7SEGMENT_DISPLAY       0xDB


//
// Accessor structure
//
// Given a WDFDEVICE handle, we'll use the following function to return
// a pointer to our device's context area.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BASICUSB_DEVICE_CONTEXT, BasicUsbGetContextFromDevice)

//
// Forward declarations
//
EVT_WDF_DRIVER_DEVICE_ADD BasicUsbEvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_READ BasicUsbEvtRead;
EVT_WDF_IO_QUEUE_IO_WRITE BasicUsbEvtWrite;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL BasicUsbEvtDeviceControl;
EVT_WDF_DEVICE_PREPARE_HARDWARE BasicUsbEvtDevicePrepareHardware;

EVT_WDF_DEVICE_D0_ENTRY BasicUsbEvtDeviceD0Entry;

EVT_WDF_DEVICE_D0_EXIT BasicUsbEvtDeviceD0Exit;


EVT_WDF_USB_READER_COMPLETION_ROUTINE BasicUsbInterruptPipeReadComplete;

EVT_WDF_REQUEST_COMPLETION_ROUTINE BasicUsbEvtRequestWriteCompletionRoutine;

EVT_WDF_REQUEST_COMPLETION_ROUTINE BasicUsbEvtRequestReadCompletionRoutine;

