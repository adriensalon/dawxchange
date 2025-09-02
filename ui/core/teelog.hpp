#pragma once

#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
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

void teelog_install(const std::function<void(const char*, std::size_t, void*)>& sink, void* user);
void teelog_uninstall();


