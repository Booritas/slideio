#include <cstring>
#include <openjpeg.h>

// These routines are added to use memory instead of a file for input and output.
//Structure need to treat memory as a stream.

typedef struct

{
    OPJ_UINT8* pData; //Our data.
    OPJ_SIZE_T dataSize; //How big is our data.
    OPJ_SIZE_T offset; //Where are we currently in our data.
} opj_memory_stream;

//This will read from our memory to the buffer.

static OPJ_SIZE_T opj_memory_stream_read(void* p_buffer, OPJ_SIZE_T p_nb_bytes, void* p_user_data)

{
    opj_memory_stream* l_memory_stream = (opj_memory_stream*)p_user_data; //Our data.
    OPJ_SIZE_T l_nb_bytes_read = p_nb_bytes; //Amount to move to buffer.
    //Check if the current offset is outside our data buffer.
    if (l_memory_stream->offset >= l_memory_stream->dataSize) return (OPJ_SIZE_T)-1;
    //Check if we are reading more than we have.
    if (p_nb_bytes > (l_memory_stream->dataSize - l_memory_stream->offset)) {
        l_nb_bytes_read = l_memory_stream->dataSize - l_memory_stream->offset; //Read all we have.
    }
    //Copy the data to the internal buffer.
    memcpy(p_buffer, &(l_memory_stream->pData[l_memory_stream->offset]), l_nb_bytes_read);
    l_memory_stream->offset += l_nb_bytes_read; //Update the pointer to the new location.
    return l_nb_bytes_read;
}

//This will write from the buffer to our memory.

static OPJ_SIZE_T opj_memory_stream_write(void* p_buffer, OPJ_SIZE_T p_nb_bytes, void* p_user_data)
{
    opj_memory_stream* l_memory_stream = (opj_memory_stream*)p_user_data; //Our data.
    OPJ_SIZE_T l_nb_bytes_write = p_nb_bytes; //Amount to move to buffer.
    //Check if the current offset is outside our data buffer.
    if (l_memory_stream->offset >= l_memory_stream->dataSize)
        return (OPJ_SIZE_T)-1;
    //Check if we are write more than we have space for.
    if (p_nb_bytes > (l_memory_stream->dataSize - l_memory_stream->offset)) {
        l_nb_bytes_write = l_memory_stream->dataSize - l_memory_stream->offset; //Write the remaining space.
    }
    //Copy the data from the internal buffer.
    memcpy(&(l_memory_stream->pData[l_memory_stream->offset]), p_buffer, l_nb_bytes_write);
    l_memory_stream->offset += l_nb_bytes_write; //Update the pointer to the new location.
    return l_nb_bytes_write;
}

//Moves the pointer forward, but never more than we have.

static OPJ_OFF_T opj_memory_stream_skip(OPJ_OFF_T p_nb_bytes, void* p_user_data)
{
    opj_memory_stream* l_memory_stream = (opj_memory_stream*)p_user_data;
    OPJ_SIZE_T l_nb_bytes;
    if (p_nb_bytes < 0)
        return -1; //No skipping backwards.
    l_nb_bytes = (OPJ_SIZE_T)p_nb_bytes; //Allowed because it is positive.
    // Do not allow jumping past the end.
    if (l_nb_bytes > l_memory_stream->dataSize - l_memory_stream->offset) {
        l_nb_bytes = l_memory_stream->dataSize - l_memory_stream->offset; //Jump the max.
    }
    //Make the jump.
    l_memory_stream->offset += l_nb_bytes;
    //Returm how far we jumped.
    return l_nb_bytes;
}

//Sets the pointer to anywhere in the memory.

static OPJ_BOOL opj_memory_stream_seek(OPJ_OFF_T p_nb_bytes, void* p_user_data)
{
    opj_memory_stream* l_memory_stream = (opj_memory_stream*)p_user_data;
    if (p_nb_bytes < 0)
        return OPJ_FALSE; //No before the buffer.
    if (p_nb_bytes > (OPJ_OFF_T)l_memory_stream->dataSize)
        return OPJ_FALSE; //No after the buffer.
    l_memory_stream->offset = (OPJ_SIZE_T)p_nb_bytes; //Move to new position.
    return OPJ_TRUE;
}

//The system needs a routine to do when finished, the name tells you what I want it to do.

static void opj_memory_stream_do_nothing(void* p_user_data)
{
    OPJ_ARG_NOT_USED(p_user_data);
}

//Create a stream to use memory as the input or output.

opj_stream_t* opj_stream_create_default_memory_stream(opj_memory_stream* p_memoryStream, OPJ_BOOL p_is_read_stream)
{
    opj_stream_t* l_stream;
    if (!(l_stream = opj_stream_default_create(p_is_read_stream)))
        return (NULL);
    //Set how to work with the frame buffer.
    if (p_is_read_stream) {
        opj_stream_set_read_function(l_stream, opj_memory_stream_read);
    }
    else {
        opj_stream_set_write_function(l_stream, opj_memory_stream_write);
    }
    opj_stream_set_seek_function(l_stream, opj_memory_stream_seek);
    opj_stream_set_skip_function(l_stream, opj_memory_stream_skip);
    opj_stream_set_user_data(l_stream, p_memoryStream, opj_memory_stream_do_nothing);
    opj_stream_set_user_data_length(l_stream, p_memoryStream->dataSize);
    return l_stream;
}
