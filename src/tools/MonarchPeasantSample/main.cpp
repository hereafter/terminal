﻿#include "pch.h"
#include <conio.h>
#include "Monarch.h"
#include "Peasant.h"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

HANDLE g_hInput = nullptr;

////////////////////////////////////////////////////////////////////////////////
std::mutex m;
std::condition_variable cv;
bool dtored = false;
winrt::weak_ref<MonarchPeasantSample::implementation::Monarch> g_weak{ nullptr };

struct MonarchFactory : implements<MonarchFactory, IClassFactory>
{
    MonarchFactory() :
        _guid{ Monarch_clsid } {};

    HRESULT __stdcall CreateInstance(IUnknown* outer, GUID const& iid, void** result) noexcept final
    {
        *result = nullptr;
        if (outer)
        {
            return CLASS_E_NOAGGREGATION;
        }

        if (!g_weak)
        {
            auto strong = make_self<MonarchPeasantSample::implementation::Monarch>();

            g_weak = (*strong).get_weak();
            return strong.as(iid, result);
        }
        else
        {
            auto strong = g_weak.get();
            return strong.as(iid, result);
        }
    }

    HRESULT __stdcall LockServer(BOOL) noexcept final
    {
        return S_OK;
    }

private:
    winrt::guid _guid;
};
////////////////////////////////////////////////////////////////////////////////

void registerAsMonarch()
{
    DWORD registrationHostClass{};
    check_hresult(CoRegisterClassObject(Monarch_clsid,
                                        make<MonarchFactory>().get(),
                                        CLSCTX_LOCAL_SERVER,
                                        // CLSCTX_LOCAL_SERVER | CLSCTX_ENABLE_AAA,
                                        // CLSCTX_LOCAL_SERVER | CLSCTX_ACTIVATE_AAA_AS_IU,
                                        // CLSCTX_LOCAL_SERVER | CLSCTX_FROM_DEFAULT_CONTEXT,
                                        // CLSCTX_LOCAL_SERVER | CLSCTX_ENABLE_CLOAKING,
                                        REGCLS_MULTIPLEUSE,
                                        &registrationHostClass));
}

winrt::MonarchPeasantSample::Monarch instantiateAMonarch()
{
    auto monarch = create_instance<winrt::MonarchPeasantSample::Monarch>(Monarch_clsid, CLSCTX_LOCAL_SERVER);
    return monarch;
}

bool areWeTheKing(const winrt::MonarchPeasantSample::Monarch& monarch, const bool logPIDs = false)
{
    auto kingPID = monarch.GetPID();
    auto ourPID = GetCurrentProcessId();
    if (logPIDs)
    {
        if (ourPID == kingPID)
        {
            printf("We're the king - our PID is %d\n", ourPID);
        }
        else
        {
            printf("we're a lowly peasant - the king is %d\n", kingPID);
        }
    }
    return (ourPID == kingPID);
}

MonarchPeasantSample::IPeasant getOurPeasant(const winrt::MonarchPeasantSample::Monarch& monarch)
{
    if (areWeTheKing(monarch))
    {
        auto iPeasant = monarch.try_as<winrt::MonarchPeasantSample::IPeasant>();
        return iPeasant;
    }
    else
    {
        auto peasant = winrt::make_self<MonarchPeasantSample::implementation::Peasant>();
        auto ourID = monarch.AddPeasant(*peasant);
        printf("The monarch assigned us the ID %d\n", ourID);
        return *peasant;
    }
}

void printPeasants(const winrt::MonarchPeasantSample::Monarch& monarch)
{
    printf("This is unimplemented\n");
}

void monarchAppLoop(const winrt::MonarchPeasantSample::Monarch& monarch,
                    const winrt::MonarchPeasantSample::IPeasant& peasant)
{
    bool exitRequested = false;
    printf("Press `l` to list peasants, `q` to quit\n");

    while (!exitRequested)
    {
        const auto ch = _getch();
        if (ch == 'l')
        {
            printPeasants(monarch);
        }
        else if (ch == 'q')
        {
            exitRequested = true;
        }
    }
}
bool peasantReadInput(const winrt::MonarchPeasantSample::Monarch& monarch,
                      const winrt::MonarchPeasantSample::IPeasant& peasant)
{
    DWORD cNumRead, fdwMode, i;
    INPUT_RECORD irInBuf[128];

    if (!ReadConsoleInput(g_hInput, // input buffer handle
                          irInBuf, // buffer to read into
                          128, // size of read buffer
                          &cNumRead)) // number of records read
    {
        printf("\x1b[31mReadConsoleInput failed\x1b[m\n");
        ExitProcess(0);
    }
    for (i = 0; i < cNumRead; i++)
    {
        switch (irInBuf[i].EventType)
        {
        case KEY_EVENT: // keyboard input
            auto key = irInBuf[i].Event.KeyEvent;
            // KeyEventProc(irInBuf[i].Event.KeyEvent);
            if (key.bKeyDown == false && key.uChar.UnicodeChar == L'q')
            {
                return true;
            }
            break;

        case MOUSE_EVENT: // mouse input
            // MouseEventProc(irInBuf[i].Event.MouseEvent);
            break;

        case WINDOW_BUFFER_SIZE_EVENT: // scrn buf. resizing
            // ResizeEventProc( irInBuf[i].Event.WindowBufferSizeEvent );
            break;

        case FOCUS_EVENT: // disregard focus events
            break;

        case MENU_EVENT: // disregard menu events
            break;

        default:
            printf("\x1b[33mUnknown event from ReadConsoleInput - this is probably impossible\x1b[m\n");
            ExitProcess(0);
            break;
        }
    }

    return false;
}
void peasantAppLoop(const winrt::MonarchPeasantSample::Monarch& monarch,
                    const winrt::MonarchPeasantSample::IPeasant& peasant)
{
    wil::unique_handle hMonarch{ OpenProcess(PROCESS_ALL_ACCESS, FALSE, monarch.GetPID()) };
    printf("handle for the monarch process is %d\n", hMonarch.get());

    g_hInput = GetStdHandle(STD_INPUT_HANDLE);
    printf("handle for the console input is %d\n", g_hInput);

    HANDLE handlesToWaitOn[2]{ hMonarch.get(), g_hInput };

    bool exitRequested = false;
    printf("Press `q` to quit\n");

    while (!exitRequested)
    {
        printf("Beginning wait...\n");
        auto waitResult = WaitForMultipleObjects(2, handlesToWaitOn, false, INFINITE);

        switch (waitResult)
        {
        // handlesToWaitOn[0] was signaled
        case WAIT_OBJECT_0 + 0:
            printf("First event was signaled.\n");
            printf("THE KING IS DEAD\n");
            exitRequested = true;
            break;

        // handlesToWaitOn[1] was signaled
        case WAIT_OBJECT_0 + 1:
            printf("Second event was signaled.\n");
            exitRequested = peasantReadInput(monarch, peasant);
            break;

        case WAIT_TIMEOUT:
            printf("Wait timed out. This should be impossible.\n");
            break;

        // Return value is invalid.
        default:
        {
            auto gle = GetLastError();
            printf("WaitForMultipleObjects returned: %d\n", waitResult);
            printf("Wait error: %d\n", gle);
            ExitProcess(0);
        }
        }
    }

    printf("Bottom of peasantAppLoop\n");
}

void app()
{
    registerAsMonarch();
    auto monarch = instantiateAMonarch();
    const bool isMonarch = areWeTheKing(monarch, true);
    auto peasant = getOurPeasant(monarch);

    if (isMonarch)
    {
        monarchAppLoop(monarch, peasant);
    }
    else
    {
        peasantAppLoop(monarch, peasant);
    }
}

int main(int argc, char** argv)
{
    auto hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);

    init_apartment();

    try
    {
        app();
    }
    catch (hresult_error const& e)
    {
        printf("Error: %ls\n", e.message().c_str());
    }

    printf("Exiting client\n");
}