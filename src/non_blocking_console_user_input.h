#pragma once
#include <mutex>
#include <queue>

class NonBlockingConsoleUserInput
{
    std::mutex mutexUserInputQueue;
    std::queue<std::string> queueUserInput;
    std::unique_ptr<std::thread> pThreadUserInput;
    std::atomic<bool>& quitFlag;
public:
    NonBlockingConsoleUserInput(std::atomic<bool>& quitFlag);
    ~NonBlockingConsoleUserInput();
public:
    // Read the next line of input from stdin, if anything is available.
    bool GetNext(std::string& result);
};
