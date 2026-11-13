#define WIN32_LEAN_AND_MEAN // Strips out networking, cryptography, and old 1990s APIs to speed up compilation
#define NOMINMAX            // Prevents Windows from defining '#define min' and '#define max' which breaks std::min and glm
#include <windows.h>

#include <chrono>
#include <filesystem>
#include <iostream>

#include "FileWatcher.h"

FileWatcher::FileWatcher() {};

FileWatcher::~FileWatcher() {
    Stop();
}

void FileWatcher::Start(const std::string& directoryPath) {
    m_running = true;
    m_directory = directoryPath;
    m_thread = std::thread(&FileWatcher::WatchLoop, this);
}

void FileWatcher::Stop() {
    m_running = false;
    if (m_hDir != nullptr && m_hDir != INVALID_HANDLE_VALUE) {
        CancelIoEx(static_cast<HANDLE>(m_hDir), NULL);
    }
    if (m_thread.joinable()) {
        m_thread.join(); // PAUSES THE RUNNING THREAD
    }
}

std::vector<std::string> FileWatcher::ConsumeModifiedFiles() {
    std::vector<std::string> output;
    // < ---------- [RAII] [MUTEX] ---------- > 
    // BRACES SAFELY LOCK THE CRITICAL SECTION
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        output.swap(m_modifiedFiles);
    }
    return output;
}

void FileWatcher::WatchLoop() {
    // < ------ [HANDLE DIRECTORY] ----- >
    m_hDir = CreateFileW(
        std::filesystem::path(m_directory).c_str(), 
        FILE_LIST_DIRECTORY,                        // TO MONITOR CHNAGES DIRECTLY
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, // FUNCTIONS EDITORS ARE STILL ALLOWED TO DO
        NULL,                                       // DEFAULT SECURITY
        OPEN_EXISTING,                              
        FILE_FLAG_BACKUP_SEMANTICS,                 // FLAG TO OPEN DIRECTORIES
        NULL
    );

    HANDLE hDir = static_cast<HANDLE>(m_hDir);
    if (hDir == INVALID_HANDLE_VALUE) {
        // FAILED TO OPEN A FOLDER
        return;
    }

    BYTE buffer[4096];
    DWORD bytesReturned;

    while (m_running) {
        BOOL success =  ReadDirectoryChangesW(
            hDir, buffer, sizeof(buffer), 
            TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,  // TYPE OF EVENTS TO LISTEN
            &bytesReturned, // HOW MANY BYTES DID WINDOWS WRITE
            NULL, // OVERLAPPED STRUCT ( SYNCHRNOUS/BLOCKING )
            NULL // COMPLETION ROUTINE ( NOT NULL WHEN ASYNCH/NON BLOCKING ) 
        );
        
        if(!success || bytesReturned == 0) break;

        // WINDOWS DEFINES A FNI STRUCT AS THIS ----- > struct FILE_NOTIFY_INFORMATION {
        //                                                      DWORD NextEntryOffset; // How many bytes to jump to the next event (0 means this is the last one!)
        //                                                      DWORD Action;          // What happened (FILE_ACTION_MODIFIED, FILE_ACTION_ADDED, etc.)
        //                                                      DWORD FileNameLength;  // Length of filename in BYTES (not characters!)
        //                                                      WCHAR FileName[1];     // The start of the wide UTF-16 character string
        //                                                  };

        FILE_NOTIFY_INFORMATION* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);

        while (fni != nullptr) {
            if (fni->Action == FILE_ACTION_MODIFIED || fni->Action == FILE_ACTION_ADDED){
                // -------------------------------------------- > EXTRACT FILE NAMES 
                int nameLengthInChars = fni->FileNameLength / sizeof(WCHAR);   
                std::wstring wideName(fni->FileName, nameLengthInChars);
                std::string fileName = std::filesystem::path(wideName).string();

                // < ------- [CRITICAL SECTION] ------- >
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_modifiedFiles.push_back(fileName);
                }
            }

            // NO MORE RECORDS LEFT
            if (fni->NextEntryOffset == 0) {
                break;
            }
            
            fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                reinterpret_cast<BYTE*>(fni) + fni->NextEntryOffset
            );
        }
    }
    
    CloseHandle(hDir);
    m_hDir = nullptr;
}