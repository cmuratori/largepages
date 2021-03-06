# largepages

This is a rudimentary test of cold-start memory paging on Windows that I put together for
[Raymond Chen's Tie](https://twitter.com/ChenCravat), who was kind enough to ask its owner to write a blog post about the architectural
reasons Windows doesn't yet have generally available large page support.
   
The idea behind this utility is to see how long it takes Windows to actually provide
memory for use under regular 4k-page demand-paged VirtualAlloc vs. "large" 2mb-page physically-locked VirtualAlloc.
This is an important metric for command-line utilities that are trying to be very fast,
such as sub-second compilers, but should be generally applicable to any scenario where
the memory footprint of an application is growing.
   
To build, use the accompanying build.bat.  It builds with MSVC by default, but you can
un-REM the clang++ line to build with clang if you have it installed.
   
### Usage

Option | Description
------------ | -------------
--large | Use 2mb pages (default: 4k pages)
--rio   | register allocated memory with [RIORegisterBuffer][] function
[number] | Set total allocation size in mb (default: 1024)
   
### Usage examples:

Command line | Description
------------ | -------------
largepages | tests provisioning of 1gb of 4k pages
largepages --large | tests provisioning of 1gb of 2mb pages
largepages 512 | tests provisioning of 512mb of 4k pages
largepages --large 256 | tests provisioning of 256mb of 2mb pages
   
### IMPORTANT

In order to test 2mb pages, you **MUST** have enabled the Windows group policy setting that allows locking memory.  If you do not do this, all attempts to test 2mb pages will fail, as Windows does not allow executables to do this in general.  If you don't know how to enable this setting, you can use [this guide from Microsoft](https://docs.microsoft.com/en-us/sql/database-engine/configure-windows/enable-the-lock-pages-in-memory-option-windows?view=sql-server-ver15) for SQL Server, but when it comes time to choose a user, choose the user who will run largepages, not the SQL service.

### Discussion

In general, this test is less about 4k vs. 2mb pages, and more about the cost of provisioning pages on demand rather than in bulk.  In Windows, 4k pages are provisioned on demand as they are touched, whereas 2mb pages are provisioned immediately upon allocation.

In this simple benchmark, 2mb pages outperform 4k pages dramatically - by numbers like 10x.  Although part of this speedup may come from the larger sizes of the pages themselves, it is likely that most of the speedup comes from the fact that 2mb pages appear to be provisioned directly in VirtualAlloc (as you would expect for physically-locked addresses).  This means they do not need to be faulted and provisioned later.  I would assume - but don't know - that a simple flag to VirtualAlloc like "MEM_USE_IMMEDIATELY" that told VirtualAlloc to make 4k pages resident right away, like ther 2mb counterparts, would make 4k performance closely resemble 2mb performance.

Alas, no such flag exists, so this is purely hypothetical at the moment.

\- [Casey](https://caseymuratori.com)

[RIORegisterBuffer]: https://docs.microsoft.com/en-us/windows/win32/api/mswsock/nc-mswsock-lpfn_rioregisterbuffer