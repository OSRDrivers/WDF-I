//
// Copyright 2007-2020 OSR Open Systems Resources, Inc.
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

#include <ntddk.h>
#include <wdf.h>

#include "NOTHING_IOCTL.h"

//
// Choose an arbitrary maximum buffer length
//
#define NOTHING_BUFFER_MAX_LENGTH 4096

//
// Nothing device context structure
//
// KMDF will associate this structure with each "Nothing" device that
// this driver creates.
//
typedef struct _NOTHING_DEVICE_CONTEXT {

    ULONG Nothing;

    //
    // Manual queues for holding incoming requests if no data
    // pending.
    //
    WDFQUEUE ReadQueue;
    WDFQUEUE WriteQueue;

}  NOTHING_DEVICE_CONTEXT, *PNOTHING_DEVICE_CONTEXT;

//
// Accessor structure
//
// Given a WDFDEVICE handle, we'll use the following function to return
// a pointer to our device's context area.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NOTHING_DEVICE_CONTEXT,
                                   NothingGetContextFromDevice)

//
// Forward declarations
//
extern "C" {
    DRIVER_INITIALIZE DriverEntry;
}
EVT_WDF_DRIVER_DEVICE_ADD          NothingEvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_READ           NothingEvtRead;
EVT_WDF_IO_QUEUE_IO_WRITE          NothingEvtWrite;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL NothingEvtDeviceControl;

EVT_WDF_DEVICE_D0_ENTRY            NothingEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT             NothingEvtDeviceD0Exit;

//
CHAR const *
NothingPowerDeviceStateToString(WDF_POWER_DEVICE_STATE DeviceState);
