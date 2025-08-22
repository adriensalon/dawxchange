#include <rtdxc/rtdxc.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// Target at least Win7; bump if you need newer
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <Windows.h>

namespace rtdxc {
namespace detail {

    namespace {

        // Tiny core used by both file_watcher_impl and directory_watcher_impl
        class WinDirWatcherCore {
        public:
            WinDirWatcherCore(const std::filesystem::path& directory,
                const std::wstring& file_filter /* empty = accept all */)
                : dir_(std::filesystem::absolute(directory))
                , filter_(file_filter)
                , running_(false)
                , handle_(INVALID_HANDLE_VALUE)
            {

                handle_ = ::CreateFileW(
                    dir_.c_str(),
                    FILE_LIST_DIRECTORY,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr,
                    OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS, // open directory
                    nullptr);

                if (handle_ == INVALID_HANDLE_VALUE) {
                    throw std::runtime_error("CreateFileW(FILE_LIST_DIRECTORY) failed");
                }

                running_.store(true, std::memory_order_release);
                worker_ = std::thread([this] { run(); });
            }

            ~WinDirWatcherCore()
            {
                stop();
            }

            void set_on_mod(std::function<void(const std::filesystem::path&)> cb)
            {
                std::lock_guard<std::mutex> lk(cb_m_);
                on_mod_ = std::move(cb);
            }
            void set_on_create(std::function<void(const std::filesystem::path&)> cb)
            {
                std::lock_guard<std::mutex> lk(cb_m_);
                on_create_ = std::move(cb);
            }
            void set_on_remove(std::function<void(const std::filesystem::path&)> cb)
            {
                std::lock_guard<std::mutex> lk(cb_m_);
                on_remove_ = std::move(cb);
            }

            void stop()
            {
                bool expected = true;
                if (running_.compare_exchange_strong(expected, false)) {
                    if (handle_ != INVALID_HANDLE_VALUE) {
                        ::CloseHandle(handle_); // unblocks ReadDirectoryChangesW
                        handle_ = INVALID_HANDLE_VALUE;
                    }
                    if (worker_.joinable())
                        worker_.join();
                }
            }

        private:
            void run()
            {
                std::vector<std::byte> buffer(64 * 1024); // common size for RDCW
                const DWORD notify_filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;

                while (running_.load(std::memory_order_acquire)) {
                    DWORD bytes = 0;
                    const BOOL ok = ::ReadDirectoryChangesW(
                        handle_,
                        buffer.data(),
                        static_cast<DWORD>(buffer.size()),
                        FALSE, // non-recursive
                        notify_filter,
                        &bytes,
                        nullptr,
                        nullptr);

                    if (!ok || bytes == 0) {
                        // handle closed or spurious wakeup
                        if (!running_.load())
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
                    std::filesystem::path full = dir_ / wname;

                    // If filtering for a single file, check it
                    if (!filter_.empty()) {
                        if (full.filename().wstring() != filter_) {
                            goto next_entry;
                        }
                    }

                    // Debounce bursts from editors
                    constexpr auto debounce = std::chrono::milliseconds(100);
                    {
                        std::lock_guard<std::mutex> lk(debounce_m_);
                        auto& last = last_emit_[full.native()]; // key by native string
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
                    std::lock_guard<std::mutex> lk(cb_m_);
                    cb = on_mod_;
                }
                if (cb)
                    cb(p);
            }
            void emit_create(const std::filesystem::path& p)
            {
                std::function<void(const std::filesystem::path&)> cb;
                {
                    std::lock_guard<std::mutex> lk(cb_m_);
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
                    std::lock_guard<std::mutex> lk(cb_m_);
                    cb = on_remove_;
                }
                if (cb)
                    cb(p);
                else
                    emit_mod(p); // fallback for file_watcher
            }

        private:
            std::filesystem::path dir_;
            std::wstring filter_; // target filename for single-file mode (empty = all)
            std::atomic<bool> running_;
            HANDLE handle_;
            std::thread worker_;

            // callbacks
            std::mutex cb_m_;
            std::function<void(const std::filesystem::path&)> on_mod_;
            std::function<void(const std::filesystem::path&)> on_create_;
            std::function<void(const std::filesystem::path&)> on_remove_;

            // debounce map
            std::mutex debounce_m_;
            std::unordered_map<std::wstring, std::chrono::steady_clock::time_point> last_emit_;
        };

    } // namespace

    // -------------------- file_watcher impl --------------------

    struct file_watcher_impl {
        explicit file_watcher_impl(const std::filesystem::path& file)
            : core(file.parent_path(), file.filename().wstring())
        {
        }

        void on_mod(const std::function<void(const std::filesystem::path&)>& cb) { core.set_on_mod(cb); }

        WinDirWatcherCore core;
    };

    file_watcher::file_watcher(const std::filesystem::path& file_path)
        : _impl(std::make_shared<file_watcher_impl>(file_path))
    {
    }

    void file_watcher::on_modification(const std::function<void(const std::filesystem::path&)>& callback)
    {
        _impl->on_mod(callback);
    }

    // -------------------- directory_watcher impl --------------------

    struct directory_watcher_impl {
        explicit directory_watcher_impl(const std::filesystem::path& dir)
            : core(dir, L"")
        {
        }

        void on_mod(const std::function<void(const std::filesystem::path&)>& cb) { core.set_on_mod(cb); }
        void on_create(const std::function<void(const std::filesystem::path&)>& cb) { core.set_on_create(cb); }
        void on_remove(const std::function<void(const std::filesystem::path&)>& cb) { core.set_on_remove(cb); }

        WinDirWatcherCore core;
    };

    directory_watcher::directory_watcher(const std::filesystem::path& directory_path)
        : _impl(std::make_shared<directory_watcher_impl>(directory_path))
    {
    }

    void directory_watcher::on_modification(const std::function<void(const std::filesystem::path&)>& callback)
    {
        _impl->on_mod(callback);
    }
    void directory_watcher::on_creation(const std::function<void(const std::filesystem::path&)>& callback)
    {
        _impl->on_create(callback);
    }
    void directory_watcher::on_removal(const std::function<void(const std::filesystem::path&)>& callback)
    {
        _impl->on_remove(callback);
    }

} // namespace detail
} // namespace rtdxc
