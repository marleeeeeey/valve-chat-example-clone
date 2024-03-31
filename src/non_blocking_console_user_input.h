#pragma once
#include <mutex>
#include <queue>

/////////////////////////////////////////////////////////////////////////////
//
// Non-blocking console user input.  Sort of.
// Why is this so hard?
//
/////////////////////////////////////////////////////////////////////////////

class NonBlockingConsoleUserInput
{
    std::mutex mutexUserInputQueue;
    std::queue<std::string> queueUserInput;
    std::thread* pThreadUserInput = nullptr;
    std::atomic<bool>& quitFlag;
public:
    NonBlockingConsoleUserInput(std::atomic<bool>& quitFlag);
    ~NonBlockingConsoleUserInput();
public:
    // Read the next line of input from stdin, if anything is available.
    bool LocalUserInput_GetNext(std::string& result);
private:
    void LocalUserInput_Init();
    void LocalUserInput_Kill();
};
