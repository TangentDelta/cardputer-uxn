#include "uxn.h"

Uxn::Uxn(const int ram_size, const int stack_size)
{
	_ram_size = ram_size;
	_stack_size = stack_size;
	_ram_mask = ram_size-1;
	_stack_mask = stack_size-1;

	// Allocate heap memory for the VM
	_ram = (uint8_t*)malloc(ram_size * sizeof(uint8_t));
	_stk[0] = (uint8_t*)malloc(stack_size * sizeof(uint8_t));
	_stk[1] = (uint8_t*)malloc(stack_size * sizeof(uint8_t));
}

Uxn::~Uxn()
{
	free(_ram);
	free(_stk[0]);
	free(_stk[1]);
}

/*
Copyright (c) 2026 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define OPC(opc, A, B) {\
	case 0x00|opc: {const uint8_t d=0,r=0;A B} break;\
	case 0x20|opc: {const uint8_t d=1,r=0;A B} break;\
	case 0x40|opc: {const uint8_t d=0,r=1;A B} break;\
	case 0x60|opc: {const uint8_t d=1,r=1;A B} break;\
	case 0x80|opc: {const uint8_t d=0,r=0,k=_ptr[0];A _ptr[0]=k;B} break;\
	case 0xa0|opc: {const uint8_t d=1,r=0,k=_ptr[0];A _ptr[0]=k;B} break;\
	case 0xc0|opc: {const uint8_t d=0,r=1,k=_ptr[1];A _ptr[1]=k;B} break;\
	case 0xe0|opc: {const uint8_t d=1,r=1,k=_ptr[1];A _ptr[1]=k;B} break;}
#define DEC(m) _stk[m][--_ptr[m] & _stack_mask]
#define INC(m) _stk[m][_ptr[m]++ & _stack_mask]
#define IMM a = _ram[pc++ & _ram_mask] << 8, a |= _ram[pc++ & _ram_mask];
#define MOV pc = d ? (uint16_t)a : pc + (int8_t)a;
#define POx(o,m) o = DEC(r); if(m) o |= DEC(r) << 8;
#define PUx(i,m,s) if(m) c = (i), INC(s) = c >> 8, INC(s) = c; else INC(s) = i;
#define GOT(o) if(d) o[1] = DEC(r); o[0] = DEC(r);
#define PUT(i,s) INC(s) = i[0]; if(d) INC(s) = i[1];
#define DEO(o,v) _deo(o, v[0]); if(d) _deo(o + 1, v[1]);
#define DEI(i,v) v[0] = _dei(i); if(d) v[1] = _dei(i + 1); PUT(v,r)
#define POK(o,v,m) _ram[o & _ram_mask] = v[0]; if(d) _ram[(o + 1) & m & _ram_mask] = v[1];
#define PEK(i,v,m) v[0] = _ram[i & _ram_mask]; if(d) v[1] = _ram[(i + 1) & m & _ram_mask]; PUT(v,r)

unsigned int Uxn::eval(uint16_t pc)
{
	unsigned int a, b, c;
	uint16_t x[2], y[2], z[2];
	for(;;)
	switch(_ram[pc++ & _ram_mask]) {
	case 0x00: return 1;
	case 0x20: if(DEC(0)) { IMM pc += a; } else pc += 2; break;
	case 0x40: IMM pc += a; break;
	case 0x60: IMM PUx(pc, 1, 1) pc += a; break;
	case 0xa0: INC(0) = _ram[pc++ & _ram_mask]; /* fall-through */
	case 0x80: INC(0) = _ram[pc++ & _ram_mask]; break;
	case 0xe0: INC(1) = _ram[pc++ & _ram_mask]; /* fall-through */
	case 0xc0: INC(1) = _ram[pc++ & _ram_mask]; break;
	OPC(0x01,POx(a,d),PUx(a+1,d,r))
	OPC(0x02,_ptr[r] -= 1+d;,{})
	OPC(0x03,GOT(x) _ptr[r] -= 1+d;,PUT(x,r))
	OPC(0x04,GOT(x) GOT(y),PUT(x,r) PUT(y,r))
	OPC(0x05,GOT(x) GOT(y) GOT(z),PUT(y,r) PUT(x,r) PUT(z,r))
	OPC(0x06,GOT(x),PUT(x,r) PUT(x,r))
	OPC(0x07,GOT(x) GOT(y),PUT(y,r) PUT(x,r) PUT(y,r))
	OPC(0x08,POx(a,d) POx(b,d),PUx(b==a,0,r))
	OPC(0x09,POx(a,d) POx(b,d),PUx(b!=a,0,r))
	OPC(0x0a,POx(a,d) POx(b,d),PUx(b>a,0,r))
	OPC(0x0b,POx(a,d) POx(b,d),PUx(b<a,0,r))
	OPC(0x0c,POx(a,d),MOV)
	OPC(0x0d,POx(a,d) POx(b,0),if(b) MOV)
	OPC(0x0e,POx(a,d),PUx(pc,1,!r) MOV)
	OPC(0x0f,GOT(x),PUT(x,!r))
	OPC(0x10,POx(a,0),PEK(a,x,0xff))
	OPC(0x11,POx(a,0) GOT(y),POK(a,y,0xff))
	OPC(0x12,POx(a,0),PEK(pc+(int8_t)a,x,0xffff))
	OPC(0x13,POx(a,0) GOT(y),POK(pc+(int8_t)a,y,0xffff))
	OPC(0x14,POx(a,1),PEK(a,x,0xffff))
	OPC(0x15,POx(a,1) GOT(y),POK(a,y,0xffff))
	OPC(0x16,POx(a,0),DEI(a,x))
	OPC(0x17,POx(a,0) GOT(y),DEO(a,y))
	OPC(0x18,POx(a,d) POx(b,d),PUx(b+a,d,r))
	OPC(0x19,POx(a,d) POx(b,d),PUx(b-a,d,r))
	OPC(0x1a,POx(a,d) POx(b,d),PUx(b*a,d,r))
	OPC(0x1b,POx(a,d) POx(b,d),PUx(a?b/a:0,d,r))
	OPC(0x1c,POx(a,d) POx(b,d),PUx(b&a,d,r))
	OPC(0x1d,POx(a,d) POx(b,d),PUx(b|a,d,r))
	OPC(0x1e,POx(a,d) POx(b,d),PUx(b^a,d,r))
	OPC(0x1f,POx(a,0) POx(b,d),PUx(b>>(a&0xf)<<(a>>4),d,r))
	} return 0;   
}

void Uxn::load(const uint8_t *rom, int count)
{
	if(count < (_ram_size-0x100))
    	memcpy(&_ram[0x100], rom, count);
	// Else handle trying to load a ROM that is too large...
}

void Uxn::set_deo_callback(uint8_t port, UxnDeviceCallback port_callback)
{
    switch(port)
    {
        case 0x18:  // Console - Write
            _cons_write = port_callback;
    }
}

void Uxn::mem_poke(uint16_t addr, uint8_t value)
{
	_ram[addr & _ram_mask] = value;
}

uint8_t Uxn::mem_peek(uint16_t addr)
{
	return _ram[addr & _ram_mask];
}

uint8_t Uxn::_dei(const uint8_t port)
{
    switch(port)
    {
        default:
            return 0;
            break;
    }
}

void Uxn::_deo(const uint8_t port, const uint8_t value)
{
    switch(port)
    {
        case 0x18:  // Console - Write
            if(_cons_write)
                _cons_write(value);
        default:
            break;
    }
}