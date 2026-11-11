#pragma once
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

class FileWatcher{
    public:
        FileWatcher();
        ~FileWatcher();
        void Start(const std::string& directoryPath);
        void Stop();

        std::vector<std::string> ConsumeModifiedFiles();

    private:
        std::mutex m_mutex;
        std::thread m_thread;
        std::vector<std::string> m_modifiedFiles;

        std::atomic<bool> m_running{false};
        void WatchLoop();

        std::string m_directory;
        void* m_hDir = nullptr;
};