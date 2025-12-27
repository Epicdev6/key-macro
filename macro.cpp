#include <windows.h>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <iostream>

struct KeyEvent {
    DWORD vk;
    bool keyDown;
    DWORD delayMs;
};

std::vector<KeyEvent> recordedEvents;
std::atomic<bool> recording(false);
std::atomic<bool> playing(false);

std::chrono::steady_clock::time_point lastEventTime;
HHOOK keyboardHook;

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;

        // Ignore injected events (prevents recursion)
        if (kb->flags & LLKHF_INJECTED)
            return CallNextHookEx(keyboardHook, nCode, wParam, lParam);

        if (recording) {
            auto now = std::chrono::steady_clock::now();
            DWORD delay = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastEventTime).count();
            lastEventTime = now;

            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                recordedEvents.push_back({ kb->vkCode, true, delay });
            } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                recordedEvents.push_back({ kb->vkCode, false, delay });
            }
        }

        // F2 = toggle recording
        if (kb->vkCode == VK_F2 && wParam == WM_KEYDOWN) {
            recording = !recording;
            if (recording) {
                recordedEvents.clear();
                lastEventTime = std::chrono::steady_clock::now();
                std::cout << "Recording started\n";
            } else {
                std::cout << "Recording stopped (" << recordedEvents.size() << " events)\n";
            }
        }

        // F3 = toggle playback
        if (kb->vkCode == VK_F3 && wParam == WM_KEYDOWN) {
            playing = !playing;
            std::cout << (playing ? "Playback started\n" : "Playback stopped\n");
        }
    }

    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

void PlayMacro() {
    while (true) {
        if (!playing || recordedEvents.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        for (const auto& e : recordedEvents) {
            if (!playing) break;

            std::this_thread::sleep_for(std::chrono::milliseconds(e.delayMs));

            INPUT input = {};
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = e.vk;
            if (!e.keyDown)
                input.ki.dwFlags = KEYEVENTF_KEYUP;

            SendInput(1, &input, sizeof(INPUT));
        }
    }
}

int main() {
    std::cout << "F2 = Start/Stop Recording\n";
    std::cout << "F3 = Start/Stop Macro\n";

    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, nullptr, 0);
    if (!keyboardHook) {
        std::cerr << "Failed to install keyboard hook\n";
        return 1;
    }

    std::thread playbackThread(PlayMacro);
    playbackThread.detach();

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(keyboardHook);
    return 0;
}
