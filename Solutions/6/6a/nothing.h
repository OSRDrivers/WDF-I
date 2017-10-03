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

#include <ntddk.h> 
#include <wdf.h>

extern "C" DRIVER_INITIALIZE DriverEntry;

//
// Generate our device interface GUID
//
#include <initguid.h> // required for GUID definitions
DEFINE_GUID (GUID_DEVINTERFACE_NOTHING,
0x79963ae7, 0x45de, 0x4a31, 0x8f, 0x34, 0xf0, 0xe8, 0x90, 0xb, 0xc2, 0x17);

#include "NOTHING_IOCTL.h"

//
// Choose an arbitrary maximum buffer length
//
#define BUFDEV_MAX_LENGTH 4096

//
// Nothing device context structure
//
// KMDF will associate this structure with each "Nothing" device that
// this driver creates.
//
// Add fields for Lab 3B.
//
typedef struct _NOTHING_DEVICE_CONTEXT  {
    ULONG   Nothing;    
    ULONG   BytesStored;
    UCHAR   Storage[BUFDEV_MAX_LENGTH];

    //
    // We'll keep all the outstanding WDFFILEOBJECTs in a
    // collection.
    //
    WDFCOLLECTION FileObjectsCollection;

    //
    // The WDFILEOBJECT of the only caller allowed to write to the
    // buffer.
    //
    WDFFILEOBJECT AllowedWriter;
} NOTHING_DEVICE_CONTEXT, * PNOTHING_DEVICE_CONTEXT;

//
// Accessor structure
//
// Given a WDFDEVICE handle, we'll use the following function to return
// a pointer to our device's context area.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NOTHING_DEVICE_CONTEXT, NothingGetContextFromDevice)

//
// Forward declarations
//
EVT_WDF_DEVICE_FILE_CREATE NothingEvtFdoCreate;
EVT_WDF_FILE_CLOSE NothingEvtFdoClose;
EVT_WDF_DRIVER_DEVICE_ADD NothingEvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_READ NothingEvtRead;
EVT_WDF_IO_QUEUE_IO_WRITE NothingEvtWrite;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL NothingEvtDeviceControl;

