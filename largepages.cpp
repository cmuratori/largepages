#include <winsock2.h>
#include <mswsock.h>
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <immintrin.h>

int EnableLargePages(void)
{
    int Result = false;

    HANDLE TokenHandle;
    if(OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &TokenHandle))
    {
        TOKEN_PRIVILEGES Privs = {};
        Privs.PrivilegeCount = 1;
        Privs.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if(LookupPrivilegeValue(0, SE_LOCK_MEMORY_NAME, &Privs.Privileges[0].Luid))
        {
            AdjustTokenPrivileges(TokenHandle, FALSE, &Privs, 0, 0, 0);
            if(GetLastError() == ERROR_SUCCESS)
            {
                Result = true;
            }
            else
            {
                fprintf(stderr, "SeLockMemoryPrivilege denied (user doesn't have group policy set?)\n");
            }
        }
        else
        {
            fprintf(stderr, "LookupPrivilegeValue failed.\n");
        }

        CloseHandle(TokenHandle);
    }
    else
    {
        fprintf(stderr, "OpenProcessToken failed.\n");
    }

    return Result;
}

int unsigned GetElapsedMS(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    LARGE_INTEGER Freq;
    QueryPerformanceFrequency(&Freq);
    int unsigned Result = (int unsigned)(1000*(End.QuadPart - Start.QuadPart) / Freq.QuadPart);
    return Result;
}

struct warm_up_thread_data {
  void *Address;
  SIZE_T Size;
};

DWORD WINAPI WarmUpThread(void *RawData) {
  warm_up_thread_data *Data = (warm_up_thread_data *)RawData;
  // note(dmitriy): Read access is enough to trigger kernel page fault and
  // zero-fill of the page memory. Not writing to memory makes it so
  // that this code can theoretically race with any other use of the memory.
  for (SIZE_T Offset = 0; Offset < Data->Size; Offset += 4096) {
    // note(dmitriy): `volatile` ensures that the dereference is not optimized away
    volatile char *Address = ((char *)Data->Address + Offset);
    *Address;
  }
  return 0;
}

int main(int ArgCount, char **Args)
{
    int Result = 0;

    int TryLargePages = false;
    int TryRio = false;
    int MultiThreaded = false;

    SIZE_T Kilobyte = 1024;
    SIZE_T Megabyte = 1024*1024;
    SIZE_T TotalSize = 1024*Megabyte;
    SIZE_T MinPageSize = 4*Kilobyte;
    SIZE_T PageSize = MinPageSize;

    //
    // NOTE(casey): "Parse" arguments
    //

    for(int ArgIndex = 1; ArgIndex < ArgCount; ++ArgIndex)
    {
        char *Arg = Args[ArgIndex];
        if(strcmp(Arg, "--large") == 0)
        {
            TryLargePages = true;
        }
        else if(strcmp(Arg, "--rio") == 0)
        {
            TryRio = true;
        }
        else if(strcmp(Arg, "--threads") == 0)
        {
            MultiThreaded = true;
        }
        else
        {
            int IsNum = true;
            for(char *Test = Arg; Test[0]; ++Test) IsNum = IsNum && ((Test[0] >= '0') && (Test[0] <= '9'));
            if(IsNum)
            {
                TotalSize = Megabyte*(SIZE_T)atoi(Arg);
            }
            else
            {
                fprintf(stderr, "Unrecognized argument \"%s\"\n", Arg);
                Result = -1;
            }
        }
    }

    //
    // NOTE(casey): Politely ask windows to enable large pages if necessary
    //

    DWORD VirtualAllocFlags = MEM_COMMIT|MEM_RESERVE;
    if(TryLargePages && EnableLargePages())
    {
        SIZE_T LargePageSize = GetLargePageMinimum();
        if(LargePageSize != 0)
        {
            PageSize = LargePageSize;
            VirtualAllocFlags |= MEM_LARGE_PAGES;
        }
    }

    //
    // NOTE(casey): Re-compute the allocation size to ensure it is an exact multiple of the page size
    //

    SIZE_T AllocSize = PageSize * ((TotalSize + PageSize - 1) / PageSize);

    RIO_EXTENSION_FUNCTION_TABLE Rio = {};
    if (TryRio)
    {
        // NOTE(mmozeiko): need to get function pointers to RIO functions, and that requires dummy socket
        WSADATA WinSockData;
        WSAStartup(MAKEWORD(2, 2), &WinSockData);

        GUID Guid = WSAID_MULTIPLE_RIO;
        DWORD RioBytes = 0;
        SOCKET Sock = socket(AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP);
        WSAIoctl(Sock, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER, &Guid, sizeof(Guid), (void**)&Rio, sizeof(Rio), &RioBytes, 0, 0);
        closesocket(Sock);
    }

    // NOTE(casey): Timed portion begins here
    // {

    // NOTE(casey): "Allocate" memory
    LARGE_INTEGER Start; QueryPerformanceCounter(&Start);
    char *Memory = (char *)VirtualAlloc(0, AllocSize, VirtualAllocFlags, PAGE_READWRITE);
    if (TryRio)
    {
        Rio.RIODeregisterBuffer(Rio.RIORegisterBuffer(Memory, AllocSize));
    };
    LARGE_INTEGER Mid; QueryPerformanceCounter(&Mid);

    if(Memory)
    {
      if (MultiThreaded)
      {
        // NOTE(dmitriy): Spin up a thread for each available core and split the walking
        enum {MAX_THREAD_COUNT = 64};
        HANDLE Threads[MAX_THREAD_COUNT];
        warm_up_thread_data ThreadData[MAX_THREAD_COUNT];

        int ThreadCount;
        {
          SYSTEM_INFO SysInfo;
          GetSystemInfo(&SysInfo);
          ThreadCount = SysInfo.dwNumberOfProcessors;
        }
        assert(ThreadCount > 0);
        if (ThreadCount > MAX_THREAD_COUNT) ThreadCount = MAX_THREAD_COUNT;

        SIZE_T ChunkSize = AllocSize / ThreadCount;
        for (int ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex) {
          ThreadData[ThreadIndex].Address = (char *)Memory + ChunkSize * ThreadIndex;
          ThreadData[ThreadIndex].Size = ChunkSize;
          Threads[ThreadIndex] = CreateThread(0, 0, WarmUpThread, &ThreadData[ThreadIndex], 0, 0);
        }
        WaitForMultipleObjects(ThreadCount, Threads, TRUE /*wait all*/, INFINITE);
      }
      else
      {
        // NOTE(casey): Force memory to actually exist via one write per page
        for(int Index = 0; Index < (AllocSize - 32); Index += MinPageSize) Memory[Index] = 1;
      }
      LARGE_INTEGER End; QueryPerformanceCounter(&End);
      // NOTE(casey): Timed portion ends here
      // }

        fprintf(stdout, "%umb via %uk pages: %ums (%ums VirtualAlloc, %ums writing)\n",
                (int unsigned)(AllocSize / Megabyte), (int unsigned)(PageSize / Kilobyte),
                GetElapsedMS(Start, End), GetElapsedMS(Start, Mid), GetElapsedMS(Mid, End));
    }
    else
    {
        fprintf(stderr, "Allocation failed.\n");
        Result = -2;
    }

    return Result;
}
