/*

	MIT License

	Copyright (c) 2021 Kento Oki & (c) 2026 EitaPreulaJhol

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.

*/

#include <windows.h>
#include <iostream>

#include "anymapper.hpp"

int wmain( int argc, const wchar_t** argv, const wchar_t** envp )
{
	if ( argc < 2 )
	{
		printf( "[=] Usage: anymapper.exe [driver_path]\n" );
		return EXIT_FAILURE;
	}

	const auto driver_path = argv[ 1 ];

	if ( !libanycall::init( "ntdll.dll", "NtTraceControl" ) )
	{
		printf( "[!] Failed to init libanycall\n" );
		return EXIT_FAILURE;
	}

	if ( !anymapper::inject_driver( driver_path ) )
	{
		printf( "[!] Failed to map driver\n" );
		return EXIT_FAILURE;
	}

    return EXIT_SUCCESS;
}