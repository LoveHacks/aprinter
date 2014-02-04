/*
 * Copyright (c) 2013 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AMBROLIB_ASF_USB_SERIAL_H
#define AMBROLIB_ASF_USB_SERIAL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/Position.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

extern "C" uint32_t udi_cdc_write_buf (const void* buf, uint32_t size);
extern "C" uint32_t udi_cdc_read_buf (void* buf, uint32_t size);
extern "C" uint32_t udi_cdc_get_nb_received_data (void);

struct AsfUsbSerialParams {};

template <typename Position, typename Context, int RecvBufferBits, int SendBufferBits, typename Params, typename RecvHandler, typename SendHandler>
class AsfUsbSerial
: private DebugObject<Context, void>
{
private:
    using RecvFastEvent = typename Context::EventLoop::template FastEventSpec<AsfUsbSerial>;
    
    static AsfUsbSerial * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
public:
    using RecvSizeType = BoundedInt<RecvBufferBits, false>;
    using SendSizeType = BoundedInt<SendBufferBits, false>;
    
    static void init (Context c, uint32_t baud)
    {
        AsfUsbSerial *o = self(c);
        
        c.eventLoop()->template initFastEvent<RecvFastEvent>(c, AsfUsbSerial::recv_event_handler);
        o->m_recv_start = RecvSizeType::import(0);
        o->m_recv_end = RecvSizeType::import(0);
        o->m_recv_overrun = false;
        o->m_recv_force = false;
        
        o->m_send_start = SendSizeType::import(0);
        o->m_send_end = SendSizeType::import(0);
        
        c.eventLoop()->template triggerFastEvent<RecvFastEvent>(c);
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        AsfUsbSerial *o = self(c);
        o->debugDeinit(c);
        
        c.eventLoop()->template resetFastEvent<RecvFastEvent>(c);
    }
    
    static RecvSizeType recvQuery (Context c, bool *out_overrun)
    {
        AsfUsbSerial *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(out_overrun)
        
        *out_overrun = o->m_recv_overrun;
        return recv_avail(o->m_recv_start, o->m_recv_end);
    }
    
    static char * recvGetChunkPtr (Context c)
    {
        AsfUsbSerial *o = self(c);
        o->debugAccess(c);
        
        return (o->m_recv_buffer + o->m_recv_start.value());
    }
    
    static void recvConsume (Context c, RecvSizeType amount)
    {
        AsfUsbSerial *o = self(c);
        o->debugAccess(c);
        
        AMBRO_ASSERT(amount <= recv_avail(o->m_recv_start, o->m_recv_end))
        o->m_recv_start = BoundedModuloAdd(o->m_recv_start, amount);
    }
    
    static void recvClearOverrun (Context c)
    {
        AsfUsbSerial *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_recv_overrun)
        
        o->m_recv_overrun = false;
    }
    
    static void recvForceEvent (Context c)
    {
        AsfUsbSerial *o = self(c);
        o->debugAccess(c);
        
        o->m_recv_force = true;
    }
    
    static SendSizeType sendQuery (Context c)
    {
        AsfUsbSerial *o = self(c);
        o->debugAccess(c);
        
        return send_avail(o->m_send_start, o->m_send_end);
    }
    
    static SendSizeType sendGetChunkLen (Context c, SendSizeType rem_length)
    {
        AsfUsbSerial *o = self(c);
        o->debugAccess(c);
        
        if (o->m_send_end.value() > 0 && rem_length > BoundedModuloNegative(o->m_send_end)) {
            rem_length = BoundedModuloNegative(o->m_send_end);
        }
        return rem_length;
    }
    
    static char * sendGetChunkPtr (Context c)
    {
        AsfUsbSerial *o = self(c);
        o->debugAccess(c);
        
        return (o->m_send_buffer + o->m_send_end.value());
    }
    
    static void sendProvide (Context c, SendSizeType amount)
    {
        AsfUsbSerial *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(amount <= send_avail(o->m_send_start, o->m_send_end))
        
        o->m_send_end = BoundedModuloAdd(o->m_send_end, amount);
    }
    
    static void sendPoke (Context c)
    {
        AsfUsbSerial *o = self(c);
        o->debugAccess(c);
        
        // we don't really care if we lose data here
        while (o->m_send_start != o->m_send_end) {
            SendSizeType amount = (o->m_send_end < o->m_send_start) ? BoundedModuloNegative(o->m_send_start) : BoundedUnsafeSubtract(o->m_send_end, o->m_send_start);
            udi_cdc_write_buf(o->m_send_buffer + o->m_send_start.value(), amount.value());
            o->m_send_start = BoundedModuloAdd(o->m_send_start, amount);
        }
    }
    
    using EventLoopFastEvents = MakeTypeList<RecvFastEvent>;
    
private:
    static RecvSizeType recv_avail (RecvSizeType start, RecvSizeType end)
    {
        return BoundedModuloSubtract(end, start);
    }
    
    static SendSizeType send_avail (SendSizeType start, SendSizeType end)
    {
        return BoundedModuloDec(BoundedModuloSubtract(start, end));
    }
    
    static void recv_event_handler (Context c)
    {
        AsfUsbSerial *o = self(c);
        o->debugAccess(c);
        
        c.eventLoop()->template triggerFastEvent<RecvFastEvent>(c);
        if (!o->m_recv_overrun) {
            RecvSizeType virtual_start = BoundedModuloDec(o->m_recv_start);
            while (o->m_recv_end != virtual_start) {
                RecvSizeType amount = (o->m_recv_end > virtual_start) ? BoundedModuloNegative(o->m_recv_end) : BoundedUnsafeSubtract(virtual_start, o->m_recv_end);
                uint32_t bytes = udi_cdc_get_nb_received_data();
                if (bytes == 0) {
                    goto no_more_data;
                }
                if (bytes > amount.value()) {
                    bytes = amount.value();
                }
                udi_cdc_read_buf(o->m_recv_buffer + o->m_recv_end.value(), bytes);
                memcpy(o->m_recv_buffer + ((size_t)RecvSizeType::maxIntValue() + 1) + o->m_recv_end.value(), o->m_recv_buffer + o->m_recv_end.value(), bytes);
                o->m_recv_end = BoundedModuloAdd(o->m_recv_end, RecvSizeType::import(bytes));
                o->m_recv_force = true;
            }
            o->m_recv_overrun = true;
            o->m_recv_force = true;
        }
    no_more_data:
        if (o->m_recv_force) {
            o->m_recv_force = false;
            RecvHandler::call(o, c);
        }
    }
    
    RecvSizeType m_recv_start;
    RecvSizeType m_recv_end;
    bool m_recv_overrun;
    bool m_recv_force;
    char m_recv_buffer[2 * ((size_t)RecvSizeType::maxIntValue() + 1)];
    
    SendSizeType m_send_start;
    SendSizeType m_send_end;
    char m_send_buffer[(size_t)SendSizeType::maxIntValue() + 1];
};

#include <aprinter/EndNamespace.h>

#endif
