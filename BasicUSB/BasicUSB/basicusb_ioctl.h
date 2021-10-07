//
// Copyright 2007-2021 OSR Open Systems Resources, Inc.
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

//
// This header file contains all declarations shared between driver and user
// applications.
//

#ifndef __BASICUSB_IOCTL_H__
#define __BASICUSB_IOCTL_H__ (1)

//
// The following value is arbitrarily chosen from the space defined by Microsoft
// as being "for non-Microsoft use"
//
#define FILE_DEVICE_BASICUSB 0xCF53

//
// Device control codes - values between 2048 and 4095 arbitrarily chosen
//
#define IOCTL_OSR_BASICUSB_SET_BAR_GRAPH CTL_CODE(FILE_DEVICE_BASICUSB,\
                                                 2049,               \
                                                 METHOD_BUFFERED,    \
                                                 FILE_WRITE_ACCESS)


#endif /* __BASICUSB_IOCTL_H__ */

