//
// Copyright 2007-2022 OSR Open Systems Resources, Inc.
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
//    This software is supplied for instructional purposes only.  It is not
//    complete, and it is not suitable for use in any production environment.
//
//    OSR Open Systems Resources, Inc. (OSR) expressly disclaims any warranty
//    for this software.  THIS SOFTWARE IS PROVIDED  "AS IS" WITHOUT WARRANTY
//    OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING, WITHOUT LIMITATION,
//    THE IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR
//    PURPOSE.  THE ENTIRE RISK ARISING FROM THE USE OF THIS SOFTWARE REMAINS
//    WITH YOU.  OSR's entire liability and your exclusive remedy shall not
//    exceed the price paid for this material.  In no event shall OSR or its
//    suppliers be liable for any damages whatsoever (including, without
//    limitation, damages for loss of business profit, business interruption,
//    loss of business information, or any other pecuniary loss) arising out
//    of the use or inability to use this software, even if OSR has been
//    advised of the possibility of such damages.  Because some states/
//    jurisdictions do not allow the exclusion or limitation of liability for
//    consequential or incidental damages, the above limitation may not apply
//    to you.
// 
#pragma once

#define BUILDING_IN_KERNEL_MODE _KERNEL_MODE

#ifndef BUILDING_IN_KERNEL_MODE

//
// Building the driver using UMDF
//
#include <Windows.h>
#include <usb.h>

#define ASSERT( exp ) \
    ((!(exp)) ? \
        (RtlAssert( (PVOID)#exp, (PVOID)__FILE__, __LINE__, NULL ),FALSE) : \
        TRUE)

#define KeGetCurrentIrql() (PASSIVE_LEVEL)

#else

//
// Building the driver using KMDF
//
#include <ntddk.h>
#include <usbdi.h>
#include <usbdlib.h>

#endif

#include <wdf.h>
#include <wdfusb.h>

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
// WDF will associate this structure with each "BasicUsb" device that
// this driver creates.
//
typedef struct _BASICUSB_DEVICE_CONTEXT  {

    //
    // Our USB I/O target
    //
    WDFUSBDEVICE UsbDeviceTarget;

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
constexpr UCHAR USBFX2LK_READ_7SEGMENT_DISPLAY = 0xD4;
constexpr UCHAR USBFX2LK_READ_SWITCHES = 0xD6;
constexpr UCHAR USBFX2LK_READ_BARGRAPH_DISPLAY = 0xD7;
constexpr UCHAR USBFX2LK_SET_BARGRAPH_DISPLAY = 0xD8;
constexpr UCHAR USBFX2LK_IS_HIGH_SPEED = 0xD9;
constexpr UCHAR USBFX2LK_REENUMERATE = 0xDA;
constexpr UCHAR USBFX2LK_SET_7SEGMENT_DISPLAY = 0xDB;

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
extern "C"  {
DRIVER_INITIALIZE DriverEntry;
}
EVT_WDF_DRIVER_DEVICE_ADD BasicUsbEvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_READ BasicUsbEvtRead;
EVT_WDF_IO_QUEUE_IO_WRITE BasicUsbEvtWrite;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL BasicUsbEvtDeviceControl;
EVT_WDF_DEVICE_PREPARE_HARDWARE BasicUsbEvtDevicePrepareHardware;

EVT_WDF_DEVICE_D0_ENTRY BasicUsbEvtDeviceD0Entry;

EVT_WDF_DEVICE_D0_EXIT BasicUsbEvtDeviceD0Exit;


EVT_WDF_USB_READER_COMPLETION_ROUTINE BasicUsbInterruptPipeReadComplete;

EVT_WDF_REQUEST_COMPLETION_ROUTINE BasicUsbEvtRequestWriteCompletionRoutine;

NTSTATUS
BasicUsbDoSetBarGraph(_In_ PBASICUSB_DEVICE_CONTEXT DevContext,
                      _In_ WDFREQUEST Request,
                      _Out_ PULONG_PTR BytesWritten);

