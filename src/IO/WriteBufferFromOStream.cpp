#include <IO/WriteBufferFromOStream.h>
#include <Common/logger_useful.h>


namespace DB
{

namespace ErrorCodes
{
    extern const int CANNOT_WRITE_TO_OSTREAM;
}

void WriteBufferFromOStream::nextImpl()
{
    if (!offset())
        return;

    ostr->write(working_buffer.begin(), offset());
    ostr->flush();

    if (!ostr->good())
    {
        /// FIXME do not call finalize in dtors (and remove iostreams)
        bool avoid_throwing_exceptions = std::uncaught_exceptions();
        if (avoid_throwing_exceptions)
            LOG_ERROR(&Poco::Logger::get("WriteBufferFromOStream"), "Cannot write to ostream at offset {}. Stack trace: {}", count(), StackTrace().toString());
        else
            throw Exception(ErrorCodes::CANNOT_WRITE_TO_OSTREAM, "Cannot write to ostream at offset {}", count());
    }
}

WriteBufferFromOStream::WriteBufferFromOStream(
    size_t size,
    char * existing_memory,
    size_t alignment)
    : BufferWithOwnMemory<WriteBuffer>(size, existing_memory, alignment)
{
}

WriteBufferFromOStream::WriteBufferFromOStream(
    std::ostream & ostr_,
    size_t size,
    char * existing_memory,
    size_t alignment)
    : BufferWithOwnMemory<WriteBuffer>(size, existing_memory, alignment), ostr(&ostr_)
{
}

WriteBufferFromOStream::~WriteBufferFromOStream()
{
    finalize();
}

}
