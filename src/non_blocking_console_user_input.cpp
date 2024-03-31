#include "non_blocking_console_user_input.h"
#include <local_utils.h>

NonBlockingConsoleUserInput::NonBlockingConsoleUserInput(std::atomic<bool>& quitFlag) : quitFlag(quitFlag)
{
    LocalUserInput_Init();
}

NonBlockingConsoleUserInput::~NonBlockingConsoleUserInput()
{
    LocalUserInput_Kill();
}

bool NonBlockingConsoleUserInput::LocalUserInput_GetNext(std::string& result)
{
    bool got_input = false;
    mutexUserInputQueue.lock();
    while (!queueUserInput.empty() && !got_input)
    {
        result = queueUserInput.front();
        queueUserInput.pop();
        LocalUtils::ltrim(result);
        LocalUtils::rtrim(result);
        got_input = !result.empty(); // ignore blank lines
    }
    mutexUserInputQueue.unlock();
    return got_input;
}

void NonBlockingConsoleUserInput::LocalUserInput_Init()
{
    pThreadUserInput = new std::thread(
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
                    LocalUtils::Printf("Failed to read on stdin, quitting\n");
                    break;
                }

                mutexUserInputQueue.lock();
                queueUserInput.push(std::string(szLine));
                mutexUserInputQueue.unlock();
            }
        });
}

void NonBlockingConsoleUserInput::LocalUserInput_Kill()
{
    // Does not work.  We won't clean up, we'll just nuke the process.
    //	g_bQuit = true;
    //	_close( fileno( stdin ) );
    //
    //	if ( s_pThreadUserInput )
    //	{
    //		s_pThreadUserInput->join();
    //		delete s_pThreadUserInput;
    //		s_pThreadUserInput = nullptr;
    //	}
}
