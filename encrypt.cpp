#include <vector>
#include <filesystem>
#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>
#include <cstdint>
#include <functional>
#include "mode_messages.h"

namespace fs = std::filesystem;

std::vector<fs::directory_entry> getTargetFiles(const Context& ctx, AppState& state) {
    std::vector<fs::directory_entry> targets;
    std::error_code ec;
    std::ofstream log(ctx.logPath, std::ios::app);
    log << "------------------------------" << std::endl;
    log << "Scanning for target files in: " << ctx.downloadsPath << std::endl;

    auto iter = fs::directory_iterator(ctx.downloadsPath, ec);
    if (ec) {
        log << "Failed to access downloads directory: " << ec.message() << std::endl;
        log << "No target files will be processed." << std::endl;
        log << "-------------------------------" << std::endl;
        return {};
    }

    for (const auto& entry : iter) {
        std::error_code read_ec;
        if (entry.is_regular_file(read_ec) && !entry.is_symlink(read_ec) && entry.path() != ctx.logPath) {
            targets.push_back(entry);
        }
    }

    log << "Found " << targets.size() << " candidate files." << std::endl;
    log << "Sorting files by last modified time." << std::endl;
    std::sort(targets.begin(), targets.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
        std::error_code time_ec;
        return a.last_write_time(time_ec) > b.last_write_time(time_ec);
    });

    uintmax_t currentTotalSize = 0;
    const uintmax_t maxSizeBytes = static_cast<uintmax_t>(ctx.sizeLimitMB) * 1024 * 1024;

    log << "Filtering files to fit within size limit: " << ctx.sizeLimitMB << " MB." << std::endl;
    auto it = targets.begin();
    while (it != targets.end()) {
        std::error_code size_ec;
        uintmax_t fileSize = it->file_size(size_ec);
        
        if (size_ec) {
            it = targets.erase(it);
            log << "Failed to get size for " << it->path() << ": " << size_ec.message() << ". Skipping file." << std::endl;
            continue;
        }
        
        if (currentTotalSize + fileSize > maxSizeBytes) {
            log << "Reached size limit with file: " << it->path() << " (size: " << (fileSize / (1024 * 1024)) << " MB). Stopping selection." << std::endl;
            targets.erase(it, targets.end());
            break;
        }
        
        currentTotalSize += fileSize;
        ++it;
    }

    log << "Selected " << targets.size() << " files for processing, total size: " << (currentTotalSize / (1024 * 1024)) << " MB." << std::endl;
    log << "Storing target file paths in AppState." << std::endl;
    it = targets.begin();
    while (it != targets.end()) {
        state.targetFiles.push_back(it->path());
        ++it;
    }

    log << "Target files:" << std::endl;
    for (const auto& file : state.targetFiles) {
        log << "  " << file << std::endl;
    }
    log << "------------------------------" << std::endl;
    return targets;
}

void copyFiles(const Context& ctx, AppState& state) {
    std::ofstream log(ctx.logPath, std::ios::app);
    log << "------------------------------" << std::endl;
    log << "Copying files to: " << ctx.downloadsPath << " with suffix: " << ctx.demoSuffix << std::endl;
    
    for (const auto& file : state.targetFiles) {
        std::error_code copy_ec;
        fs::path destination = fs::path(ctx.downloadsPath) / (file.filename().stem().string() + ctx.demoSuffix + file.filename().extension().string());
        fs::copy_file(file, destination, fs::copy_options::overwrite_existing, copy_ec);
        
        if (copy_ec) {
            std::cerr << "Failed to copy " << file << " to " << destination << ": " << copy_ec.message() << std::endl;
        }

        state.copyFiles.push_back(destination);
    }
    log << "Copied " << state.copyFiles.size() << " files." << std::endl;
    log << "------------------------------" << std::endl;
    log << "------------------------------" << std::endl;
    int n = 0;
    for (const auto& file : state.copyFiles) {
        log << "COPY_FILE=" << file << std::endl;
        ++n;
    }
    log << "COPY_COUNT=" << n << std::endl;
    log << "------------------------------" << std::endl;
}

void hideFiles(const std::vector<fs::directory_entry>& files) {
    // TODO: Implement file hiding logic (platform-specific)
    // May be scrapped if deemed too complex or unreliable across platforms
    // Not technically necessary since copies will be encrypted and original files can be left as is, but may be a nice touch if implemented correctly
    // Copies will also appear above originals due to being newer
}

void xorFiles(const Context& ctx, AppState& state) { // Symmetric XOR encryption for demonstration purposes only, not secure for real use
    std::ofstream log(ctx.logPath, std::ios::app);
    log << "------------------------------" << std::endl;
    log << "Encrypting files with XOR stream cipher." << std::endl;
    
    for (const auto& filePath : state.copyFiles) {
        if (!fs::exists(filePath)) continue;
        log << "Encrypting file: " << filePath << std::endl;

        uint64_t fileSize = static_cast<uint64_t>(fs::file_size(filePath));
        uint64_t streamState = state.encryptionKey ^ fileSize;

        std::fstream file(filePath, std::ios::in | std::ios::out | std::ios::binary);

        std::vector<char> buffer(4096);
        while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
            std::streamsize bytesRead = file.gcount();
            for (std::streamsize i = 0; i < bytesRead; ++i) {
                buffer[i] ^= static_cast<char>(streamState & 0xFF);
                streamState = (streamState >> 8) | ((streamState & 0xFF) << 56);
            }
            file.seekp(-bytesRead, std::ios::cur);
            file.write(buffer.data(), bytesRead);
            file.seekg(file.tellp());
        }

        file.close();
        log << "Finished encrypting: " << filePath << std::endl;
    }

    log << "Encryption complete for " << state.copyFiles.size() << " files." << std::endl;
    log << "------------------------------" << std::endl;
}

UiRequest encrypt_start(const Context& ctx, AppState& state) {
    std::ofstream log(ctx.logPath, std::ios::app);
    log << "------------------------------" << std::endl;
    log << "Starting Encrypt Mode." << std::endl;
    state.encryptPhase = EncryptPhase::Warning;
    log << "ENCRYPT_PHASE=WARNING" << std::endl;
    log << "------------------------------" << std::endl;

    return UiRequest::MakeMessage(
        "Encrypt Mode",
        "This demo will simulate encrypting files in your Downloads folder by copying them, appending a suffix, and applying a simple XOR cipher to the copies. The original files will be left unchanged. This is for demonstration purposes only and is NOT secure encryption. \n\n"
        "Press Next to begin scanning for target files.",
        "Next"
    );
}

UiRequest encrypt_step(const Context& ctx, AppState& state, const UserInput& input) {
    std::ofstream log(ctx.logPath, std::ios::app);
    log << "------------------------------" << std::endl;
    log << "Encrypt Mode: Received user input. Current phase: " << static_cast<int>(state.encryptPhase) << std::endl;

    if (input.kind != InputKind::PrimaryButton) {
        log << "Unexpected input kind. Expected PrimaryButton." << std::endl;
        log << "------------------------------" << std::endl;
        return UiRequest::MakeMessage("Encrypt Mode", "Expected primary button input.");
    }
    
    switch (state.encryptPhase) {
        case EncryptPhase::Warning:
            log << "Transitioning to SCANNING phase." << std::endl;
            log << "ENCRYPT_PHASE=SCANNING" << std::endl;
            log << "--------------------------------" << std::endl;

            state.encryptPhase = EncryptPhase::Scanning;
            getTargetFiles(ctx, state);
            return UiRequest::MakeMessage(
                "Scanning Complete", 
                "Found " + std::to_string(state.targetFiles.size()) + " files to process. Press Next to create demo copies.", 
                "Next");

        case EncryptPhase::Scanning:
            log << "Transitioning to COPYING phase." << std::endl;
            log << "ENCRYPT_PHASE=COPYING" << std::endl;
            log << "--------------------------------" << std::endl;

            state.encryptPhase = EncryptPhase::Copying;
            copyFiles(ctx, state);
            return UiRequest::MakeMessage(
                "Copying Complete", 
                "Created " + std::to_string(state.copyFiles.size()) + " demo copies. Press Next to encrypt the copies.", 
                "Next");

        case EncryptPhase::Copying:
            log << "Transitioning to ENCRYPTING phase." << std::endl;
            log << "ENCRYPT_PHASE=ENCRYPTING" << std::endl;
            log << "--------------------------------" << std::endl;

            state.encryptPhase = EncryptPhase::Encrypting;
            xorFiles(ctx, state);
            return UiRequest::MakeMessage(
                "Encryption Complete", 
                "Demo files have been encrypted. Original files are unchanged. Press Next to finish.", 
                "Next");
        case EncryptPhase::Encrypting:
            log << "Transitioning to DONE phase." << std::endl;
            log << "ENCRYPT_PHASE=DONE" << std::endl;
            log << "--------------------------------" << std::endl;

            state.encryptPhase = EncryptPhase::Done;
            return UiRequest::MakeNavigate(Mode::Educate, "Encryption demo complete. Navigating to Educate mode.");
        default:
            log << "Unexpected encryption phase." << std::endl;
            log << "------------------------------" << std::endl;
            return UiRequest::MakeMessage("Encrypt Mode", "Unexpected state.");
        }
}