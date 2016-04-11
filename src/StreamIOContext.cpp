#include <StreamIOContext.hpp>

#include <gbBase/Assert.hpp>

extern "C" {
#   include <libavformat/avio.h>
}

#include <istream>

StreamIOContext::StreamIOContext(std::istream& stream, std::size_t buffer_size)
    :m_fin(stream),
     m_io_buffer(static_cast<unsigned char*>(av_malloc(buffer_size)), [](void*) { /* will be freed by the io_context */ }),
     m_io_context_storage(avio_alloc_context(m_io_buffer.get(), static_cast<int>(buffer_size), 0, this,
                                             StreamIOContext::static_io_read_packet, nullptr,
                                             StreamIOContext::static_io_seek),
                          [](AVIOContext* ctx) { av_free(ctx->buffer); av_free(ctx); })
{
}

AVIOContext* StreamIOContext::getContext() const
{
    return m_io_context_storage.get();
}

int StreamIOContext::static_io_read_packet(void* fstr, uint8_t* buf, int buf_size)
{
    if(buf_size == 0) { return 0; }
    auto* self = static_cast<StreamIOContext*>(fstr);
    return self->io_read_packet(buf, buf_size);
};

int StreamIOContext::io_read_packet(uint8_t* buf, int buf_size)
{
    m_fin.read(reinterpret_cast<char*>(buf), buf_size);
    if(m_fin.fail() && !m_fin.eof()) { 
        return -1;
    }
    int ret = static_cast<int>(m_fin.gcount());
    return ret;
}

int64_t StreamIOContext::static_io_seek(void* fstr, int64_t offset, int whence)
{
    auto* self = static_cast<StreamIOContext*>(fstr);
    return self->io_seek(offset, whence);
};

int64_t StreamIOContext::io_seek(int64_t offset, int whence)
{
    int64_t ret;
    switch(whence)
    {
    case SEEK_SET:
        m_fin.seekg(offset, std::ios_base::beg);
        ret = m_fin.tellg();
        break;
    case SEEK_CUR:
        m_fin.seekg(offset, std::ios_base::cur);
        ret = m_fin.tellg();
        break;
    case SEEK_END:
        m_fin.seekg(offset, std::ios_base::end);
        ret = m_fin.tellg();
        break;
    case AVSEEK_SIZE:
    {
        auto pos = m_fin.tellg();
        m_fin.seekg(0, std::ios_base::end);
        auto pos_end = m_fin.tellg();
        m_fin.seekg(0, std::ios_base::beg);
        auto fsize = pos_end - m_fin.tellg();
        m_fin.seekg(pos);
        ret = fsize;
    } break;
    default:
        GHULBUS_UNREACHABLE_MESSAGE("Invalid whence token in seek.");
    }
    if(m_fin.fail()) { return -1; }
    return ret;
}

