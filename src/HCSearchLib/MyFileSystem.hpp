#ifndef MYFILESYSTEM_HPP
#define MYFILESYSTEM_HPP

#include <iostream>
#include <string>
#include <cstdlib>
using namespace std;

namespace MyFileSystem
{
	/**************** FileSystem ****************/

	class FileSystem
	{
	public:

	};

	/**************** Executable ****************/

	// For executing command line
	class Executable
	{
	protected:
		// Default number of retries
		static const int DEFAULT_NUM_RETRIES;

	public:
		// Call shell to execute command cmd
		static int execute(string cmd);

		// Call shell to execute command cmd
		// Retry if necessary
		static int executeRetries(string cmd, int numRetries);

		// Call shell to execute command cmd
		// Retry if necessary (default num retries)
		static int executeRetries(string cmd);
	};
}

#endif