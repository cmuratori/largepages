/* NOTE(casey):

   This is a rudimentary test of cold-start memory paging on Windows that I put together for
   Raymond Chen, who was kind enough to offer to write a blog post about the architectural
   reasons Windows doesn't yet have generally available large page support.
   
   The idea behind this utility is to see how long it takes Windows to actually provide
   memory for use under regular 4k-page VirtualAlloc vs. "large" 2mb-page VirtualAlloc.
   This is an important metric for command-line utilities that are trying to be very fast,
   such as sub-second compilers, but should be generally applicable to any scenario where
   the memory footprint of an application is growing.
   
   To build, use the accompanying build.bat.  It builds with MSVC by default, but you can
   un-REM the clang++ line to build with clang if you have it installed.
   
   Usage examples:
   
   largepages                          ; tests provisioning of 1gb of 4k pages
   largepages --large                  ; tests provisioning of 1gb of 2mb pages
   largepages 512                      ; tests provisioning of 512mb of 4k pages
   largepages -large 256               ; tests provisioning of 256mb of 2mb pages
   
   -- IMPORTANT --
   IN ORDER TO TEST 2MB PAGES, YOU **MUST** HAVE ENABLED THE WINDOWS GROUP POLICY SETTING
   THAT ALLOWS LOCKING LARGE PAGES.  IF YOU DO NOT DO THIS, ALL ATTEMPTS TO TEST 2MB PAGES
   WILL FAIL.
   ---------------
*/

#include <windows.h>
#include <stdio.h>
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

int main(int ArgCount, char **Args)
{
    int Result = 0;
    
    int TryLargePages = false;
    
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
    
    // NOTE(casey): Timed portion begins here
    // {
    
    // NOTE(casey): "Allocate" memory
    LARGE_INTEGER Start; QueryPerformanceCounter(&Start);
    char *Memory = (char *)VirtualAlloc(0, AllocSize, VirtualAllocFlags, PAGE_READWRITE);
    LARGE_INTEGER Mid; QueryPerformanceCounter(&Mid);
    
    if(Memory)
    {
        // NOTE(casey): Force memory to actually exist via one write per page
        for(int Index = 0; Index < (AllocSize - 32); Index += MinPageSize) Memory[Index] = 1;
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