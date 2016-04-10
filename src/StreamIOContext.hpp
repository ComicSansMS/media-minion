#pragma once

#include <cstdint>
#include <iosfwd>
#include <memory>

struct AVIOContext;

class StreamIOContext {
private:
    std::istream& m_fin;
    std::unique_ptr<unsigned char, void(*)(void*)> m_io_buffer;
    std::unique_ptr<AVIOContext, void(*)(AVIOContext*)> m_io_context_storage;
public:
    StreamIOContext(std::istream& stream, std::size_t buffer_size);
    AVIOContext* getContext() const;
private:
    static int static_io_read_packet(void* fstr, uint8_t* buf, int buf_size);
    static int64_t static_io_seek(void* fstr, int64_t offset, int whence);
    int io_read_packet(uint8_t* buf, int buf_size);
    int64_t io_seek(int64_t offset, int whence);
};
