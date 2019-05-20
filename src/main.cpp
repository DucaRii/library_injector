#include <Windows.h>

#include <filesystem>
#include <TlHelp32.h>
#include <iostream>
#include <thread>

/// Simple loadlibrary injector

#define DLL_NAME "target_lib.dll"
#define PROCESS_NAME "target_process.exe"

/// Usage: Either just start the program
///        and it will inject DLL_NAME ( if found )
///        Or you drop a file onto the program and
///        it will inject that file

/// <summary>
/// Loops through all processes and searches for the provided process
/// </summary>
/// <param name="name">Process name to search for</param>
/// <returns>Process ID, 0 if not found</returns>
uint32_t get_process_info( const char* name )
{
	auto snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );

	if ( snapshot == INVALID_HANDLE_VALUE )
		return false;

	auto entry = PROCESSENTRY32{ sizeof( PROCESSENTRY32 ) };

	if ( Process32First( snapshot, &entry ) )
	{
		do
		{
			if ( !strcmp( entry.szExeFile, name ) )
			{
				CloseHandle( snapshot );
				return entry.th32ProcessID;				
			}
		} while ( Process32Next( snapshot, &entry ) );
	}

	CloseHandle( snapshot );
	return 0;
}

/// <summary>
/// Injects a .dll file utilizing kernel32's LoadLibrary function and mapping it into the process
/// </summary>
/// <param name="arg_number">Argument number provided with the call of the function</param>
/// <param name="arguments">Arguments provided with the call of the function</param>
/// <returns>Success of function</returns>
BOOL main( int arg_number, char* arguments[] )
{
	HANDLE process_handle;
	LPVOID path_alloc;
	HANDLE remote_handle;

	auto cleanup = [ & ]() -> void
	{
		if ( path_alloc )
			VirtualFreeEx( process_handle, path_alloc, 0, MEM_RELEASE );

		if ( remote_handle )
			CloseHandle( remote_handle );

		if ( process_handle )
			CloseHandle( process_handle );
	};

	try
	{
		auto file_path = std::string( "" );

		/// Check if the program was called with a custom name
		/// arg number 1 = current file path
		/// arg number 2 = destination file path
		if ( arg_number == 2 )
		{
			file_path = arguments[ 1 ];
		}
		else
		{
			file_path = std::filesystem::canonical( DLL_NAME ).string();
		}

		/// Check for file existance ( No need for this really since
		/// canonical will handle that but whatever )
		if ( !std::filesystem::exists( file_path ) )
			throw std::exception( "Failed to find target file" );

		/// Check if process exists
		auto process_id = get_process_info( PROCESS_NAME );
		if ( !process_id )
			throw std::exception( "Target process isn't open" );

		/// Now get a handle to the process
		process_handle = OpenProcess( PROCESS_ALL_ACCESS, FALSE, process_id );
		if ( !process_handle )
			throw std::exception( "Failed to open process handle" );

		/// Finally inject the file into the target process

		/// Find the LoadLibrary function
		auto kernel_handle = GetModuleHandleA( "kernel32.dll" );
		if ( !kernel_handle )
			throw std::exception( "Failed to open kernel32 handle" );

		auto load_library = reinterpret_cast< LPVOID >( GetProcAddress( kernel_handle, "LoadLibraryA" ) );
		if ( !load_library )
			throw std::exception( "Failed to find LoadLibrary" );

		/// Allocate memory in target process for our file path
		path_alloc = VirtualAllocEx( process_handle, NULL, file_path.size(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
		if ( !path_alloc )
			throw std::exception( "Failed to allocate memory for file path" );

		/// Write the file path into the allocated memory
		WriteProcessMemory( process_handle, path_alloc, file_path.c_str(), file_path.size(), NULL );

		/// Now load the dll by calling LoadLIbrary in the target process
		/// with our allocated file path
		remote_handle = CreateRemoteThread( process_handle, NULL, NULL, reinterpret_cast< LPTHREAD_START_ROUTINE >( load_library ), path_alloc, NULL, NULL );
		if ( !remote_handle )
			throw std::exception( "Failed to open remote handle" );

		std::cout << "Successfully injected!" << std::endl;

		std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );

		/// Cleanup memory/handles
		cleanup();
	}
	catch ( std::exception & ex )
	{
		/// Cleanup memory/handles
		cleanup();

		MessageBoxA( NULL, ex.what(), "Error", MB_OK | MB_ICONERROR );

		return TRUE;
	}

	return TRUE;
}