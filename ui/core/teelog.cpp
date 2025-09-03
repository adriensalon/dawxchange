#include "teelog.hpp"

#include <iostream>
#include <streambuf>

std::vector<std::string> teelog_ring::snapshot() const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(lock_mutex));
    return std::vector<std::string>(lines.begin(), lines.end());
}

void teelog_ring::push_line(std::string line)
{
    std::lock_guard<std::mutex> lock(lock_mutex);
    lines.push_back(std::move(line));
    if (lines.size() > max_lines)
        lines.pop_front();
}

void teelog_ring::clear()
{
    std::lock_guard<std::mutex> lock(lock_mutex);
    lines.clear();
}

struct teelog_buffer : public std::streambuf {

    teelog_buffer(std::streambuf* a, const std::function<void(const char*, std::size_t, void*)>& sink, void* user)
        : a_(a)
        , sink_(sink)
        , user_(user)
    {
        setp(buf_, buf_ + sizeof(buf_));
    }

    ~teelog_buffer() override
    {
        sync();
        flush_linebuf();
    }

protected:
    // write a block
    std::streamsize xsputn(const char* line, std::streamsize n) override
    {
        // forward to original
        auto n1 = a_ ? a_->sputn(line, n) : n;
        // split into lines and feed sink
        feed_sink(line, static_cast<std::size_t>(n));
        return n1;
    }

    // write a single char
    int overflow(int ch) override
    {
        if (ch == EOF)
            return sync();
        if (a_ && a_->sputc(static_cast<char>(ch)) == EOF)
            return EOF;
        char c = static_cast<char>(ch);
        feed_sink(&c, 1);
        return ch;
    }

    int sync() override
    {
        if (a_)
            a_->pubsync();
        return 0;
    }

private:
    void feed_sink(const char* line, std::size_t n)
    {
        for (std::size_t i = 0; i < n; ++i) {
            linebuf_.push_back(line[i]);
            if (line[i] == '\n')
                flush_linebuf();
        }
    }

    void flush_linebuf()
    {
        if (linebuf_.empty())
            return;
        if (sink_)
            sink_(linebuf_.data(), linebuf_.size(), user_);
        linebuf_.clear();
    }

    std::streambuf* a_;
    std::function<void(const char*, std::size_t, void*)> sink_;
    void* user_;
    char buf_[256];
    std::string linebuf_;
};

static std::streambuf* old_buffer = nullptr;
static teelog_buffer* buffer;

void teelog_install(const std::function<void(const char*, std::size_t, void*)>& sink, void* user)
{
    if (buffer)
        return;
    old_buffer = std::clog.rdbuf();
    buffer = new teelog_buffer(old_buffer, sink, user);
    std::clog.setf(std::ios::unitbuf); // flush after each insertion
    std::clog.rdbuf(buffer);
}

void teelog_uninstall()
{
    if (!buffer)
        return;
    std::clog.rdbuf(old_buffer);
    delete buffer;
    buffer = nullptr;
    old_buffer = nullptr;
}

