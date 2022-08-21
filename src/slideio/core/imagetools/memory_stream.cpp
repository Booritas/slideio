// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <algorithm>
#include "slideio/core/imagetools/memory_stream.hpp"
#include <openjpeg.h>
#include <string.h>
// https://github.com/DraconPern/fmjpeg2koj/blob/master/memory_file.cpp
// is used for impllementation of this file and the corresponded header
// file

using namespace slideio;

OPJ_SIZE_T readFromMemory(void *buffer, OPJ_SIZE_T bytes, slideio::OPJStreamUserData* data);
OPJ_SIZE_T writeToMemory (void *buffer, OPJ_SIZE_T bytes, OPJStreamUserData* data);
OPJ_OFF_T skipMemory (OPJ_OFF_T bytes, OPJStreamUserData * data);
OPJ_BOOL seekMemory (OPJ_OFF_T bytes, OPJStreamUserData * data);

OPJ_SIZE_T readFromMemory(void * buffer, OPJ_SIZE_T bytes, OPJStreamUserData* data)
{		
	if (!data || !data->data || data->size == 0 || data->offset >= data->size) {
		return -1;
	}
	OPJ_SIZE_T bufferLength = data->size - data->offset;
	const OPJ_SIZE_T readlength = bytes < bufferLength ? bytes : bufferLength;
	memcpy(buffer, &data->data[data->offset], readlength);
	data->offset += readlength;
	return readlength;
}

OPJ_SIZE_T writeToMemory (void * buffer, OPJ_SIZE_T bytes, OPJStreamUserData* data)
{	
	if (!data || !data->data || data->size == 0 || data->offset >= data->size) {
	    return -1;
	}
	OPJ_SIZE_T bufferLength = data->size - data->offset;
	OPJ_SIZE_T writeLength = bytes < bufferLength ? bytes : bufferLength;
	memcpy(&data->data[data->offset], buffer, writeLength);
	data->offset += writeLength;
	return writeLength;
}

OPJ_OFF_T skipMemory (OPJ_OFF_T bytes, OPJStreamUserData * data)
{	
	if (!data || !data->data || data->size == 0 || bytes < 0) {
		return -1;
	}

	const OPJ_SIZE_T newoffset = data->offset + bytes;

	if(newoffset > data->size) {
		bytes = data->size - data->offset;
		data->offset = data->size;		
	}
	else {
		data->offset = newoffset;		
	}
	return bytes;
}

OPJ_BOOL seekMemory (OPJ_OFF_T bytes, OPJStreamUserData * data)
{
	if (!data || !data->data || data->size == 0 || bytes < 0) {
		return OPJ_FALSE;
	}
	data->offset = std::min((OPJ_SIZE_T) bytes, data->size);	
	return OPJ_TRUE;
}

opj_stream_t* slideio::createOPJMemoryStream(OPJStreamUserData* userData, size_t size, bool input )
{
	opj_stream_t* stream(nullptr);
	if (!userData)
		return nullptr;

	stream = opj_stream_create(size, input?OPJ_TRUE:OPJ_FALSE);
	if (!stream)
		return nullptr;

	opj_stream_set_user_data(stream, userData, nullptr);
	opj_stream_set_user_data_length(stream, userData->size);
	opj_stream_set_read_function(stream,(opj_stream_read_fn) readFromMemory);
	opj_stream_set_write_function(stream, (opj_stream_write_fn) writeToMemory);
	opj_stream_set_skip_function(stream, (opj_stream_skip_fn) skipMemory);
	opj_stream_set_seek_function(stream, (opj_stream_seek_fn) seekMemory);
	return stream;
}
