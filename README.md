# library_injector
Inject dynamic link libraries (.dll) into a target process

```
#define DLL_NAME "target_lib.dll"
#define PROCESS_NAME "target_process.exe"

/// Usage: Either just start the program
///        and it will inject DLL_NAME ( if found )
///        Or you drop a file onto the program and
///        it will inject that file
```
