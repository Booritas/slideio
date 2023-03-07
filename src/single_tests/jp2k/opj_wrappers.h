#include <openjpeg.h>


class wopj_cparameters : public opj_cparameters_t
{
public:
    wopj_cparameters() {
        opj_cparameters_t* str = (opj_cparameters_t*)this;
        memset(str, 0, sizeof(opj_cparameters_t));
        opj_set_default_encoder_parameters(this);
    }
    ~wopj_cparameters() {
        if (cp_comment) {
            free(cp_comment);
        }
        if (cp_matrice) {
            free(cp_matrice);
        }
    }
};

class CodecPtr {
public:
    CodecPtr(opj_codec_t* codec = nullptr) : m_codec(codec) {}

    CodecPtr(const CodecPtr& other) {
        m_codec = other.m_codec;
        const_cast<CodecPtr&>(other).m_codec = nullptr;
    }

    CodecPtr& operator=(const CodecPtr& other) {
        if (this != &other) {
            if (m_codec) {
                opj_destroy_codec(m_codec);
            }
            m_codec = other.m_codec;
            const_cast<CodecPtr&>(other).m_codec = nullptr;
        }
        return *this;
    }

    ~CodecPtr() {
        if (m_codec) {
            opj_destroy_codec(m_codec);
        }
    }

    opj_codec_t* get() const {
        return m_codec;
    }

    operator opj_codec_t* () const {
        return m_codec;
    }

private:
    opj_codec_t* m_codec;
};

class ImagePtr {
public:
    ImagePtr(opj_image_t* image = nullptr) : m_image(image) {}

    ImagePtr(const ImagePtr& other) {
        m_image = other.m_image;
        const_cast<ImagePtr&>(other).m_image = nullptr;
    }

    ImagePtr& operator=(const ImagePtr& other) {
        if (this != &other) {
            if (m_image) {
                opj_image_destroy(m_image);
            }
            m_image = other.m_image;
            const_cast<ImagePtr&>(other).m_image = nullptr;
        }
        return *this;
    }

    ~ImagePtr() {
        if (m_image) {
            opj_image_destroy(m_image);
        }
    }

    opj_image_t* get() const {
        return m_image;
    }

    operator opj_image_t* () const {
        return m_image;
    }

private:
    opj_image_t* m_image;
};

class StreamPtr {
public:
    StreamPtr(opj_stream_t* codec = nullptr) : m_stream(codec) {}

    StreamPtr(const StreamPtr& other) {
        m_stream = other.m_stream;
        const_cast<StreamPtr&>(other).m_stream = nullptr;
    }

    StreamPtr& operator=(const StreamPtr& other) {
        if (this != &other) {
            if (m_stream) {
                opj_stream_destroy(m_stream);
            }
            m_stream = other.m_stream;
            const_cast<StreamPtr&>(other).m_stream = nullptr;
        }
        return *this;
    }

    ~StreamPtr() {
        if (m_stream) {
            opj_stream_destroy(m_stream);
        }
    }

    opj_stream_t* get() const {
        return m_stream;
    }

    operator opj_stream_t* () const {
        return m_stream;
    }

private:
    opj_stream_t* m_stream;
};
