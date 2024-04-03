#include "non_blocking_console_user_input.h"
#include "my_cpp_utils/logger.h"
#include "my_cpp_utils/string_utils.h"
#include <memory>
#include <mutex>
#include <thread>

NonBlockingConsoleUserInput::NonBlockingConsoleUserInput(std::atomic<bool>& quitFlag_) : quitFlag(quitFlag_)
{
    pThreadUserInput = std::make_unique<std::thread>(
        [this]()
        {
            while (!quitFlag)
            {
                char szLine[4000];
                if (!fgets(szLine, sizeof(szLine), stdin))
                {
                    // Well, you would hope that you could close the handle
                    // from the other thread to trigger this.  Nope.
                    if (quitFlag)
                        return;
                    quitFlag = true;
                    MY_LOG(warn, "Failed to read on stdin, quitting");
                    break;
                }

                mutexUserInputQueue.lock();
                queueUserInput.push(std::string(szLine));
                mutexUserInputQueue.unlock();
            }
        });
}

NonBlockingConsoleUserInput::~NonBlockingConsoleUserInput()
{
    quitFlag = true;

    if (!pThreadUserInput)
        return;

    pThreadUserInput->join();
}

bool NonBlockingConsoleUserInput::GetNext(std::string& result)
{
    bool got_input = false;
    std::lock_guard<std::mutex> lock{mutexUserInputQueue};
    while (!queueUserInput.empty() && !got_input)
    {
        result = queueUserInput.front();
        queueUserInput.pop();
        utils::Trim(result);
        got_input = !result.empty(); // ignore blank lines
    }
    return got_input;
}
