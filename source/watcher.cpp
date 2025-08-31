#include <rtdxc/rtdxc.hpp>

#include <windows.h>

#include <atomic>
#include <mutex>
#include <thread>

namespace rtdxc {
namespace detail {

    namespace {

        struct win32_watcher {

            win32_watcher(
                const std::filesystem::path& directory,
                const std::wstring& file_filter = L"")
                : _directory(std::filesystem::absolute(directory))
                , _filter(file_filter)
                , _is_running(false)
                , _handle(INVALID_HANDLE_VALUE)
            {
                _handle = ::CreateFileW(
                    _directory.c_str(),
                    FILE_LIST_DIRECTORY,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr,
                    OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS, // open directory
                    nullptr);

                if (_handle == INVALID_HANDLE_VALUE) {
                    throw std::runtime_error("CreateFileW(FILE_LIST_DIRECTORY) failed");
                }

                _is_running.store(true, std::memory_order_release);
                _worker = std::thread([this] { run(); });
            }

            ~win32_watcher()
            {
                stop();
            }

            void set_on_mod(std::function<void(const std::filesystem::path&)> cb)
            {
                std::lock_guard<std::mutex> lk(_callback_mutex);
                on_mod_ = std::move(cb);
            }

            void set_on_create(std::function<void(const std::filesystem::path&)> cb)
            {
                std::lock_guard<std::mutex> lk(_callback_mutex);
                on_create_ = std::move(cb);
            }

            void set_on_remove(std::function<void(const std::filesystem::path&)> cb)
            {
                std::lock_guard<std::mutex> lk(_callback_mutex);
                on_remove_ = std::move(cb);
            }

            void stop()
            {
                bool expected = true;
                if (_is_running.compare_exchange_strong(expected, false)) {
                    if (_handle != INVALID_HANDLE_VALUE) {
                        ::CloseHandle(_handle); // unblocks ReadDirectoryChangesW
                        _handle = INVALID_HANDLE_VALUE;
                    }
                    if (_worker.joinable())
                        _worker.join();
                }
            }

        private:
            // static constexpr auto debounce = std::chrono::milliseconds(100);

            void run()
            {
                std::vector<std::byte> buffer(64 * 1024); // common size for RDCW
                const DWORD notify_filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;

                while (_is_running.load(std::memory_order_acquire)) {
                    DWORD bytes = 0;
                    const BOOL ok = ::ReadDirectoryChangesW(
                        _handle,
                        buffer.data(),
                        static_cast<DWORD>(buffer.size()),
                        FALSE, // non-recursive
                        notify_filter,
                        &bytes,
                        nullptr,
                        nullptr);

                    if (!ok || bytes == 0) {
                        // handle closed or spurious wakeup
                        if (!_is_running.load())
                            break;
                        continue;
                    }

                    const auto now = std::chrono::steady_clock::now();
                    parse_and_emit(buffer.data(), bytes, now);
                }
            }

            void parse_and_emit(void* data, DWORD bytes, std::chrono::steady_clock::time_point now)
            {
                auto* base = static_cast<const std::byte*>(data);
                DWORD offset = 0;

                while (offset < bytes) {
                    auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(base + offset);

                    // Extract the file name (FileNameLength is in BYTES)
                    std::wstring wname(info->FileName, info->FileName + (info->FileNameLength / sizeof(WCHAR)));
                    std::filesystem::path full = _directory / wname;

                    // If filtering for a single file, check it
                    if (!_filter.empty()) {
                        if (full.filename().wstring() != _filter) {
                            goto next_entry;
                        }
                    }

                    // Debounce bursts from editors
                    static constexpr auto debounce = std::chrono::milliseconds(100);
                    {
                        std::lock_guard<std::mutex> lk(_debounce_mutex);
                        auto& last = _last_emit[full.native()]; // key by native string
                        if (now - last < debounce)
                            goto next_entry;
                        last = now;
                    }

                    switch (info->Action) {
                    case FILE_ACTION_ADDED:
                    case FILE_ACTION_RENAMED_NEW_NAME:
                        emit_create(full);
                        break;
                    case FILE_ACTION_REMOVED:
                    case FILE_ACTION_RENAMED_OLD_NAME:
                        emit_remove(full);
                        break;
                    case FILE_ACTION_MODIFIED:
                    default:
                        emit_mod(full);
                        break;
                    }

                next_entry:
                    if (info->NextEntryOffset == 0)
                        break;
                    offset += info->NextEntryOffset;
                }
            }

            void emit_mod(const std::filesystem::path& p)
            {
                std::function<void(const std::filesystem::path&)> cb;
                {
                    std::lock_guard<std::mutex> lk(_callback_mutex);
                    cb = on_mod_;
                }
                if (cb)
                    cb(p);
            }
            void emit_create(const std::filesystem::path& p)
            {
                std::function<void(const std::filesystem::path&)> cb;
                {
                    std::lock_guard<std::mutex> lk(_callback_mutex);
                    cb = on_create_;
                }
                if (cb)
                    cb(p);
                else
                    emit_mod(p); // fallback for file_watcher
            }
            void emit_remove(const std::filesystem::path& p)
            {
                std::function<void(const std::filesystem::path&)> cb;
                {
                    std::lock_guard<std::mutex> lk(_callback_mutex);
                    cb = on_remove_;
                }
                if (cb)
                    cb(p);
                else
                    emit_mod(p); // fallback for file_watcher
            }

        private:
            std::filesystem::path _directory;
            std::wstring _filter; // target filename for single-file mode (empty = all)
            std::atomic<bool> _is_running;
            HANDLE _handle;
            std::thread _worker;

            std::mutex _callback_mutex;
            std::function<void(const std::filesystem::path&)> on_mod_;
            std::function<void(const std::filesystem::path&)> on_create_;
            std::function<void(const std::filesystem::path&)> on_remove_;

            std::mutex _debounce_mutex;
            std::unordered_map<std::wstring, std::chrono::steady_clock::time_point> _last_emit;
        };

    }

    struct file_watcher_impl {
        explicit file_watcher_impl(const std::filesystem::path& file)
            : core(file.parent_path(), file.filename().wstring())
        {
        }

        void on_modification(const std::function<void(const std::filesystem::path&)>& cb) { core.set_on_mod(cb); }

        win32_watcher core;
    };

    struct directory_watcher_impl {
        explicit directory_watcher_impl(const std::filesystem::path& directory)
            : core(directory)
        {
        }

        void on_modification(const std::function<void(const std::filesystem::path&)>& cb) { core.set_on_mod(cb); }
        void on_create(const std::function<void(const std::filesystem::path&)>& cb) { core.set_on_create(cb); }
        void on_remove(const std::function<void(const std::filesystem::path&)>& cb) { core.set_on_remove(cb); }

        win32_watcher core;
    };

    file_watcher::file_watcher(const std::filesystem::path& file_path)
        : _impl(std::make_shared<file_watcher_impl>(file_path))
    {
    }

    void file_watcher::on_modification(const std::function<void(const std::filesystem::path&)>& callback)
    {
        _impl->on_modification(callback);
    }

    directory_watcher::directory_watcher(const std::filesystem::path& directory_path)
        : _impl(std::make_shared<directory_watcher_impl>(directory_path))
    {
    }

    void directory_watcher::on_modification(const std::function<void(const std::filesystem::path&)>& callback)
    {
        _impl->on_modification(callback);
    }

    void directory_watcher::on_creation(const std::function<void(const std::filesystem::path&)>& callback)
    {
        _impl->on_create(callback);
    }

    void directory_watcher::on_removal(const std::function<void(const std::filesystem::path&)>& callback)
    {
        _impl->on_remove(callback);
    }

}
}
