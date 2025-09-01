#pragma once

#include <deque>
#include <functional>
#include <mutex>
#include <iostream>
#include <streambuf>
#include <string>
#include <vector>


struct teelog_ring {
    std::mutex lock_mutex;
    std::deque<std::string> lines;
    std::size_t max_lines = 5000;

    [[nodscard]] std::vector<std::string> snapshot() const;
    void push_line(std::string line);
    void clear();
};

struct teelog {
    void install(const std::function<void(const char*, std::size_t, void*)>& sink, void* user);
    void uninstall();

private:
    std::streambuf* old_buffer = nullptr;
    struct teelog_buffer* buffer = nullptr;
};
