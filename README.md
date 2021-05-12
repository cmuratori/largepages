# largepages

This is a rudimentary test of cold-start memory paging on Windows that I put together for
[Raymond Chen's Tie](https://twitter.com/ChenCravat), who was kind enough to ask his owner to write a blog post about the architectural
reasons Windows doesn't yet have generally available large page support.
   
The idea behind this utility is to see how long it takes Windows to actually provide
memory for use under regular 4k-page VirtualAlloc vs. "large" 2mb-page VirtualAlloc.
This is an important metric for command-line utilities that are trying to be very fast,
such as sub-second compilers, but should be generally applicable to any scenario where
the memory footprint of an application is growing.
   
To build, use the accompanying build.bat.  It builds with MSVC by default, but you can
un-REM the clang++ line to build with clang if you have it installed.
   
### Usage

Option | Description
------------ | -------------
--large | Use 2mb pages (default: 4k pages)
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
