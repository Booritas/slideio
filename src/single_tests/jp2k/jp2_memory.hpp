#include <openjpeg.h>

typedef struct
{
	OPJ_UINT8* pData; //Our data.
	OPJ_SIZE_T dataSize; //How big is our data.
	OPJ_SIZE_T offset; //Where are we currently in our data.
} opj_memory_stream;

opj_stream_t* opj_stream_create_default_memory_stream(opj_memory_stream* memoryStream, OPJ_BOOL is_read_stream);
