#include "Recorder.hpp"
#include <fstream>
#include <mmsystem.h>

using namespace geode::prelude;

namespace lr {

bool Mic::available() { return waveInGetNumDevs() > 0; }

bool Mic::start() {
    if (m_active) discard();
    if (!available()) return false;
    {
        std::lock_guard lk(m_mutex);
        m_pcm.clear();
    }
    m_active = true;
    m_thread = std::thread([this] { run(); });
    return true;
}

void Mic::run() {
    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = 44100;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = 44100 * 2;

    HWAVEIN h = nullptr;
    if (waveInOpen(&h, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        m_active = false;
        return;
    }
    constexpr int N = 4;
    constexpr DWORD SZ = 4410 * 2; // 100 ms
    std::vector<char> bufs[N];
    WAVEHDR hdr[N]{};
    for (int i = 0; i < N; i++) {
        bufs[i].resize(SZ);
        hdr[i].lpData = bufs[i].data();
        hdr[i].dwBufferLength = SZ;
        waveInPrepareHeader(h, &hdr[i], sizeof(WAVEHDR));
        waveInAddBuffer(h, &hdr[i], sizeof(WAVEHDR));
    }
    waveInStart(h);
    while (m_active) {
        for (int i = 0; i < N; i++) {
            if (hdr[i].dwFlags & WHDR_DONE) {
                {
                    std::lock_guard lk(m_mutex);
                    auto s = reinterpret_cast<int16_t*>(hdr[i].lpData);
                    m_pcm.insert(m_pcm.end(), s, s + hdr[i].dwBytesRecorded / 2);
                }
                waveInUnprepareHeader(h, &hdr[i], sizeof(WAVEHDR));
                hdr[i].dwBytesRecorded = 0;
                hdr[i].dwFlags = 0;
                waveInPrepareHeader(h, &hdr[i], sizeof(WAVEHDR));
                waveInAddBuffer(h, &hdr[i], sizeof(WAVEHDR));
            }
        }
        Sleep(10);
    }
    waveInStop(h);
    waveInReset(h);
    for (int i = 0; i < N; i++) waveInUnprepareHeader(h, &hdr[i], sizeof(WAVEHDR));
    waveInClose(h);
}

void Mic::discard() {
    m_active = false;
    if (m_thread.joinable()) m_thread.join();
    std::lock_guard lk(m_mutex);
    m_pcm.clear();
}

bool Mic::stopAndSave(std::filesystem::path const& wav) {
    m_active = false;
    if (m_thread.joinable()) m_thread.join();
    std::lock_guard lk(m_mutex);
    if (m_pcm.empty()) return false;

    std::ofstream f(wav, std::ios::binary);
    if (!f) return false;
    uint32_t dataBytes = static_cast<uint32_t>(m_pcm.size() * 2);
    auto w32 = [&](uint32_t v) { f.write(reinterpret_cast<char*>(&v), 4); };
    auto w16 = [&](uint16_t v) { f.write(reinterpret_cast<char*>(&v), 2); };
    f.write("RIFF", 4); w32(36 + dataBytes); f.write("WAVE", 4);
    f.write("fmt ", 4); w32(16); w16(1); w16(1); w32(44100); w32(44100 * 2); w16(2); w16(16);
    f.write("data", 4); w32(dataBytes);
    f.write(reinterpret_cast<char*>(m_pcm.data()), dataBytes);
    m_pcm.clear();
    return true;
}

}
