#include "uxn.h"

Uxn::Uxn(const int ram_size, const int stack_size)
{
	_ram_size = ram_size;
	_stack_size = stack_size;
	_ram_mask = ram_size-1;
	_stack_mask = stack_size-1;
}

Uxn::~Uxn()
{
	// Free up the file handles
	if(_file_handle[0])
		_file_handle[0].close();
	if(_file_handle[1])
		_file_handle[1].close();
	
	// Free up the heap memory that we allocated
	free(_ram);
	free(_stk[0]);
	free(_stk[1]);
}

bool Uxn::begin()
{
	int free = heap_caps_get_free_size(MALLOC_CAP_8BIT);

	if(free < (_ram_size + (_stack_size*2)))
		return false;
	
	// Allocate heap memory for the VM
	_ram = (uint8_t*)malloc(_ram_size * sizeof(uint8_t));
	_stk[0] = (uint8_t*)malloc(_stack_size * sizeof(uint8_t));
	_stk[1] = (uint8_t*)malloc(_stack_size * sizeof(uint8_t));

	memset(_ram, 0, _ram_size);
	memset(_devices, 0, 256);

	return true;
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
	{
		uint8_t op = _ram[pc++ & _ram_mask];
		switch(op) {
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
		}
	}
	return 0;   
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
		case 0x16:	// Console - stty
			_console_stty = port_callback; break;
        case 0x18:  // Console - Write
            _console_write = port_callback; break;
		case 0x19:	// Console - error
			_console_error = port_callback; break;
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

/*
Call the Uxn instance's console vector with the specified value
Optionally provide the value type as well
*/
void Uxn::console_vector(uint8_t value, ConsoleType value_type)
{
	_devices[0x12] = value;
	_devices[0x17] = value_type;
	uint16_t console_vector_ptr = _devices[0x10]<<8;
	console_vector_ptr |= _devices[0x11];
	if(console_vector_ptr != 0)
		eval(console_vector_ptr);
}

// Get the null-terminated string pointed at by File/name*
const char *Uxn::_get_filename(uint8_t *device)
{
	uint16_t addr = device[FileDevicePorts::NAME_HI] << 8;
	addr |= device[FileDevicePorts::NAME_LO];
	const char* file_name = (char*)_ram+addr;

	// If the file name pointed at isn't null-terminated, make sure we clamp the size and return an empty file name
	if(strlen(file_name) > 128)
		return "";

	return file_name;
}

// Close the file handle given the index
void Uxn::_file_close(uint8_t file_index)
{
	_file_handle[file_index].close();
	_file_handle_state[file_index] = FileHandleState::closed;
}

// Attempt to read a file from the filesystem
void Uxn::_file_read(uint8_t *device, uint8_t file_index)
{
	File& file_handle = _file_handle[file_index];

	// If this file handle hasn't been set up yet, try opening the file for reading
	if(!file_handle)
		file_handle = sd_card_handler.open(_get_filename(device), "r");

	// Can the file be opened?
	if(!file_handle)
	{
		// No, set the number of read bytes to 0 and return
		device[FileDevicePorts::SUCCESS_HI] = 0;
		device[FileDevicePorts::SUCCESS_LO] = 0;
		return;
	}

	_file_handle_state[file_index] = FileHandleState::open_read;

	// Is this actually a directory?
	if(file_handle.isDirectory())
	{
		// Populate the destination with the directory table
		_file_dir_content(device, file_index);
		return;
	}

	// This code feels redundant with _file_dir_content...
	// TODO: Can this code be consolidated?
	uint16_t buffer_length = device[FileDevicePorts::LENGTH_HI] << 8;
	buffer_length |= device[FileDevicePorts::LENGTH_LO];
	uint16_t dest_addr = device[FileDevicePorts::READ_HI] << 8;
	dest_addr |= device[FileDevicePorts::READ_LO];
	uint16_t bytes_written = 0;

	while(file_handle.available())
	{
		if(bytes_written < buffer_length)
		{
			char c = file_handle.read();
			_ram[dest_addr+bytes_written] = c;
			bytes_written++;
		}
		else
		{
			// Ran out of buffer. Report how many bytes we wrote and return
			// This will keep the file_handle open for future reading
			device[FileDevicePorts::SUCCESS_HI] = bytes_written>>8;
			device[FileDevicePorts::SUCCESS_LO] = bytes_written&0xff;
			return;
		}
	}
	// If we're here, the file ran out of bytes for us

	// Report how many bytes we were able to read
	device[FileDevicePorts::SUCCESS_HI] = bytes_written>>8;
	device[FileDevicePorts::SUCCESS_LO] = bytes_written&0xff;
}

void Uxn::_file_write(uint8_t *device, uint8_t file_index)
{
	File& file_handle = _file_handle[file_index];

	// If the file handle was previously open for reading, close it
	if(_file_handle_state[file_index] == FileHandleState::open_read)
		file_handle.close();

	// Initialize bytes written at 0
	device[FileDevicePorts::SUCCESS_HI] = 0;
	device[FileDevicePorts::SUCCESS_LO] = 0;

	if(!file_handle)
	{
		const char *file_name = _get_filename(device);
		const char *end = file_name;	// A pointer to the null terminator in the filename
		while(*(end++) != '\0');	// Compute the offset to the null terminator in the filename

		// If the filename is invalid, immediately return
		if((end-file_name) == 0) return;
		// Create all of the directories needed to open this file
		sd_card_handler.create_dirs(file_name);
		// If the filename was a directory, our job here is done and we can return with 0 bytes written
		if(end[-1] == '/') return;

		file_handle = sd_card_handler.open(file_name, device[FileDevicePorts::APPEND] != 0 ? "a" : "w");
	}

	uint16_t buffer_length = device[FileDevicePorts::LENGTH_HI] << 8;
	buffer_length |= device[FileDevicePorts::LENGTH_LO];
	uint16_t source_addr = device[FileDevicePorts::WRITE_HI] << 8;
	source_addr |= device[FileDevicePorts::WRITE_LO];
	uint16_t bytes_written = 0;
	for(int i = 0; i < buffer_length; i++)
		bytes_written+=file_handle.write(_ram[(source_addr+i)&_ram_mask]);

	// Report how many bytes we were able to write
	device[FileDevicePorts::SUCCESS_HI] = bytes_written>>8;
	device[FileDevicePorts::SUCCESS_LO] = bytes_written&0xff;
}

void Uxn::_file_stat(uint8_t *device, uint8_t file_index)
{

}

void Uxn::_file_dir_content(uint8_t *device, uint8_t file_index)
{
	// This assumes that _file_handle[file_index] is not null, and points at a directory!

	// Set some variables with data from the device
	uint16_t buffer_length = device[FileDevicePorts::LENGTH_HI] << 8;
	buffer_length |= device[FileDevicePorts::LENGTH_LO];
	uint16_t dest_addr = device[FileDevicePorts::READ_HI] << 8;
	dest_addr |= device[FileDevicePorts::READ_LO];

	// Directory listing variables
	uint16_t bytes_written = 0;
	char dir_entry_buffer[32];

	File& file_handle = _file_handle[file_index];

	File f = file_handle.openNextFile();
	while(f)
	{
		if(f.isDirectory())
			snprintf(dir_entry_buffer, sizeof(dir_entry_buffer), "---- %.25s/\n", f.name());
		else
		{
			unsigned long file_size = f.size();
			if(file_size > 0xffff)
				snprintf(dir_entry_buffer, sizeof(dir_entry_buffer), "???? %.26s\n", f.name());
			else
				snprintf(dir_entry_buffer, sizeof(dir_entry_buffer), "%04x %.26s\n", file_size, f.name());
		}


		for(int i=0; i < 32; i++)
		{
			if(bytes_written < buffer_length)
			{
				char c = dir_entry_buffer[i];
				if(c == '\0')
					break;
				_ram[dest_addr+(bytes_written++)] = c;
			}
			else
			{
				device[FileDevicePorts::SUCCESS_HI] = bytes_written>>8;
				device[FileDevicePorts::SUCCESS_LO] = bytes_written&0xff;
				f.close();
				return;
			}
			
		}

		f = file_handle.openNextFile();
	}

	f.close();
	device[FileDevicePorts::SUCCESS_HI] = bytes_written>>8;
	device[FileDevicePorts::SUCCESS_LO] = bytes_written&0xff;
}

uint8_t Uxn::_dei(const uint8_t port)
{
	switch(port)
	{
		case 0x04:	return _ptr[0]; // System - wst
		case 0x05:	return _ptr[1]; // System - rst
	}
	return _devices[port];
}

void Uxn::_deo(const uint8_t port, const uint8_t value)
{
	_devices[port] = value;
    switch(port)
    {
		case 0x04:	_ptr[0] = value; break; // System - wst
		case 0x05:	_ptr[1] = value; break; // System - rst
		case 0x0f:	// System - State
			alive = (value == 0); break;
		case 0x10:	// Console - Vector
		case 0x11:
			console_vector_set = true;
			alive = true;	// Mark this Uxn instance as alive and having a vector
			break;
		case 0x16:	// Console - Stty
			if(_console_stty) _console_stty(value); return;
        case 0x18:  // Console - Write
            if(_console_write) _console_write(value); return;
		case 0x19:	// Console - Error
			if(_console_error) _console_error(value); return;
		case 0xa5:	_file_stat(_devices+0xa0, 0); break;	// File A stat
		case 0xa7:	// File A append (closes file handle A if open)
		case 0xa9: if(_file_handle[0]) _file_close(0); break; // File A name (closes file handle A if open)
		case 0xad: _file_read(_devices+0xa0, 0); break;	// File A read
		case 0xaf: _file_write(_devices+0xa0, 0); break;	// File B write
		case 0xb5:	_file_stat(_devices+0xb0, 0); break;	// File B stat
		case 0xb7:	// File B append (closes file handle B if open)
		case 0xb9: if(_file_handle[1]) _file_close(1); break; // File B name (closes file handle B if open)
		case 0xbd: _file_read(_devices+0xb0, 1); break;	// File A read
		case 0xbf: _file_write(_devices+0xb0, 1); break;	// File B write
        default:
            break;
    }
}