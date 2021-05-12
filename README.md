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
