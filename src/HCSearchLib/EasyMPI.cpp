#include "EasyMPI.h"
#include <queue>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include "MyLogger.hpp"
#include "Globals.hpp"

namespace EasyMPI
{
	using namespace HCSearch;

	const int EasyMPI::MAX_MESSAGE_SIZE = 128;
	const int EasyMPI::MAX_NUM_PROCESSES = 512;
	const string EasyMPI::MASTER_FINISH_MESSAGE = "MASTERFINISHEDALLTASKS";
	const string EasyMPI::SLAVE_FINISH_MESSAGE = "SLAVEFINISHEDTASK";

	void EasyMPI::masterScheduleTasks(vector<string> commands, vector<string> messages)
	{
		char recvbuff[MAX_MESSAGE_SIZE];
		const int numTasks = commands.size();
		const int numProcesses = Global::settings->NUM_PROCESSES;
		const int rank = Global::settings->RANK;

		if (numProcesses == 1)
		{
			LOG(WARNING) << "Cannot run master-slave with one process!" << endl;
			return;
		}

		if (commands.size() != messages.size())
		{
			LOG(ERROR) << "Commands size does not equal messages size." << endl;
			abort(1);
		}

		if (numTasks == 0)
		{
			LOG(WARNING) << "No tasks. Nothing to process.";
		}
		else
		{
			// state variables
			vector<bool> finishedTasks; // maintain which tasks are completed
			vector<int> processTask; // maintain which process is assigned to a task
			queue<int> unassignedTasks; // maintain queue of tasks waiting to be processed
			queue<int> availableProcesses; // maintain queue of available processes for work

			// initialize state
			for (int i = 0; i < numTasks; i++)
			{
				finishedTasks.push_back(false);
				unassignedTasks.push(i);
			}
			processTask.push_back(-1); // process 0
			for (int i = 1; i < numProcesses; i++)
			{
				availableProcesses.push(i);
				processTask.push_back(-1);
			}

			// assign as many tasks to processes as possible
			while (!availableProcesses.empty())
			{
				if (unassignedTasks.empty())
					break;

				// get available process
				int slaveID = availableProcesses.front();
				availableProcesses.pop();

				// get task
				int taskID = unassignedTasks.front();
				unassignedTasks.pop();

				// assign task to available process by sending message to slave
				LOG(INFO) << "Master is assigning task to slave [" << slaveID << "/" << numProcesses << "].";
				string fullMessage = constructFullMessage(commands[taskID], messages[taskID]);
				const char* fullMessageString = fullMessage.c_str();
				int ierr = MPI_Send(const_cast<char*>(fullMessageString), MAX_MESSAGE_SIZE, MPI_CHAR, slaveID, 0, MPI_COMM_WORLD);

				// update state
				processTask[slaveID] = taskID;
			}

			// wait for messages until all tasks are assigned and completed
			while (true)
			{
				int msgFlag = 0;
				MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &msgFlag, Global::settings->MPI_STATUS);

				// if a messsage is available
				if (msgFlag)
				{
					// get the message tag and especially the source
					int messageID = (*Global::settings->MPI_STATUS).MPI_TAG;
					int messageSource = (*Global::settings->MPI_STATUS).MPI_SOURCE;
					LOG(INFO) << "A message from process [" << messageSource << "/" << numProcesses << "].";

					// receive the message into the buffer and check if it is the correct (slave finish) command
					int ierr = MPI_Recv(recvbuff, MAX_MESSAGE_SIZE, MPI_CHAR, messageSource, 0, MPI_COMM_WORLD, Global::settings->MPI_STATUS);
				
					// process full message into command and message components
					string fullMessage = recvbuff;
					string command;
					string message;
					bool success = parseFullMessage(fullMessage, command, message);

					// check if correct (slave finish) message
					const int commandSize = command.length();
					bool correctMessage = true;
					for (int i = 0; i < commandSize; i++)
					{
						if (command.at(i) != SLAVE_FINISH_MESSAGE.at(i))
						{
							correctMessage = false;
							break;
						}
					}
		
					// if correct message, update state and check what else needs to be done
					if (correctMessage)
					{
						LOG(INFO) << "Master received finished message from slave [" << messageSource << "/" << numProcesses << "].";
					
						// get completed task ID
						int taskID = processTask[messageSource];

						// sanity check
						if (taskID < 0 || taskID >= numTasks)
						{
							LOG(ERROR) << "Task ID '" << taskID << "' gotten is invalid!";
							abort(1);
						}

						// update state
						processTask[messageSource] = -1;
						finishedTasks[taskID] = true;
						availableProcesses.push(messageSource);
					
						// check if any other tasks need to be processed
						// otherwise check all tasks are completed
						if (!unassignedTasks.empty())
						{
							// get available process
							int slaveID = availableProcesses.front();
							availableProcesses.pop();

							// get task
							int taskID = unassignedTasks.front();
							unassignedTasks.pop();

							// assign task to available process by sending message to slave
							LOG(INFO) << "Master is assigning task to slave [" << slaveID << "/" << numProcesses << "].";
							string fullMessage = constructFullMessage(commands[taskID], messages[taskID]);
							const char* fullMessageString = fullMessage.c_str();
							int ierr = MPI_Send(const_cast<char*>(fullMessageString), MAX_MESSAGE_SIZE, MPI_CHAR, slaveID, 0, MPI_COMM_WORLD);

							// update state
							processTask[slaveID] = taskID;
						}
						else
						{
							// test if every task is finished
							bool allFinish = true;
							for (int i = 0; i < numTasks; i++)
							{
								if (!finishedTasks[i])
								{
									LOG(INFO) << "Task " << i << " is still being processed...";
									allFinish = false;
								}
							}
							if (allFinish)
							{
								break;
							}
						}
					}
				}
			}
			LOG(INFO) << "All tasks are finished!";
		}

		// everything finished, so send finish command to all slaves
		for (int slaveID = 1; slaveID < numProcesses; slaveID++)
		{
			LOG(INFO) << "Master is telling slave [" << slaveID << "/" << numProcesses << "] that all tasks are done.";
			string fullMessage = constructFullMessage(MASTER_FINISH_MESSAGE, "");
			const char* fullMessageString = fullMessage.c_str();
			int ierr = MPI_Send(const_cast<char*>(fullMessageString), MAX_MESSAGE_SIZE, MPI_CHAR, slaveID, 0, MPI_COMM_WORLD);
		}
	}

	void EasyMPI::slaveWaitForTasks(string& command, string& message)
	{
		const int numProcesses = Global::settings->NUM_PROCESSES;
		const int rank = Global::settings->RANK;
		
		if (numProcesses == 1)
			return;
		
		char recvbuff[MAX_MESSAGE_SIZE];

		// wait until get a message from master
		// will block until a message comes from the master!
		while (true)
		{
			// wait for message from master
			int ierr = MPI_Recv(recvbuff, MAX_MESSAGE_SIZE, MPI_CHAR, 0, 0, MPI_COMM_WORLD, Global::settings->MPI_STATUS);

			// process full message into command and message components
			string fullMessage = recvbuff;
			bool success = parseFullMessage(fullMessage, command, message);

			if (success)
			{
				LOG(INFO) << "Slave [" << rank << "/" << numProcesses << "]" << " got the command '" 
					<< command << "' and message '" << message << "' from master.";

				break;
			}
		}
	}

	void EasyMPI::slaveFinishedTask()
	{
		const int numProcesses = Global::settings->NUM_PROCESSES;
		const int rank = Global::settings->RANK;

		if (numProcesses == 1)
			return;

		// send master the finished message
		LOG(INFO) << "Slave [" << rank << "/" << numProcesses << "] is telling master it has finished a task.";
		string fullMessage = constructFullMessage(SLAVE_FINISH_MESSAGE, "");
		const char* fullMessageString = fullMessage.c_str();
		int ierr = MPI_Send(const_cast<char*>(fullMessageString), MAX_MESSAGE_SIZE, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
	}

	void EasyMPI::synchronize(string slaveBroadcastMsg, string masterBroadcastMsg)
	{
		masterWait(slaveBroadcastMsg);
		slavesWait(masterBroadcastMsg);
	}

	void EasyMPI::masterWait(string slaveBroadcastMsg)
	{
		// receive buffer
		char recvbuff[MAX_MESSAGE_SIZE];
		const char* SLAVEBROADCASTMSG = slaveBroadcastMsg.c_str();
		const int SLAVEBROADCASTMSG_SIZE = slaveBroadcastMsg.length();
	
		// master needs to wait here until all slaves reaches here
		const int numProcesses = Global::settings->NUM_PROCESSES;
		const int rank = Global::settings->RANK;
		if (rank == 0)
		{
			LOG(INFO) << "Master process [" << rank << "/" << numProcesses << "] is waiting to get " << slaveBroadcastMsg << " message from all slaves...";

			// wait until all slaves are done
			bool finish[MAX_NUM_PROCESSES];
			finish[0] = true;
			for (int i = 1; i < numProcesses; i++)
			{
				finish[i] = false;
			}

			// master process is now waiting for all slave processes to finish
			while (true)
			{
				if (numProcesses <= 1)
					break;

				int msgFlag = 0;
				MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &msgFlag, Global::settings->MPI_STATUS);

				// if a messsage is available
				if (msgFlag)
				{
					// get the message tag and especially source
					int messageID = (*Global::settings->MPI_STATUS).MPI_TAG;
					int messageSource = (*Global::settings->MPI_STATUS).MPI_SOURCE;
					LOG(INFO) << "A message from process [" << messageSource << "/" << numProcesses << "].";

					// receive the message into the buffer and check if it is the correct command
					int ierr = MPI_Recv(recvbuff, SLAVEBROADCASTMSG_SIZE, MPI_CHAR, messageSource, 0, MPI_COMM_WORLD, Global::settings->MPI_STATUS);
					
					// check if correct message
					bool correctMessage = true;
					for (int i = 0; i < SLAVEBROADCASTMSG_SIZE; i++)
					{
						if (recvbuff[i] != slaveBroadcastMsg[i])
						{
							correctMessage = false;
							break;
						}
					}
					
					if (correctMessage)
					{
						LOG(INFO) << "Received " << slaveBroadcastMsg << " message from process [" << messageSource << "/" << numProcesses << "].";
						finish[messageSource] = true;

						// test if every process is finished
						bool allFinish = true;
						for (int i = 1; i < numProcesses; i++)
						{
							if (!finish[i])
							{
								LOG(INFO) << "Process [" << i << "/" << numProcesses << "] is still not here yet...";
								allFinish = false;
							}
						}
						if (allFinish)
						{
							break;
						}
						LOG(INFO) << "Still waiting for all slaves to get here with the master...";
					}
				}
			}
		
			// MASTER CAN DO STUFF HERE BEFORE SLAVES PROCEED

			LOG(INFO) << "Master process [" << rank << "/" << numProcesses << "] has continued...";
		}
		else
		{
			LOG(INFO) << "Slave process [" << rank << "/" << numProcesses << "] is sending arrival message " << SLAVEBROADCASTMSG << " to master...";

			// send finish heuristic learning message to master
			int ierr = MPI_Send(const_cast<char*>(SLAVEBROADCASTMSG), SLAVEBROADCASTMSG_SIZE, MPI_CHAR, 0, 0, MPI_COMM_WORLD);

			LOG(INFO) << "Slave process [" << rank << "/" << numProcesses << "] has continued...";
		}
	}

	void EasyMPI::slavesWait(string masterBroadcastMsg)
	{
		// receive buffer
		char recvbuff[MAX_MESSAGE_SIZE];
		const char* MASTERBROADCASTMSG = masterBroadcastMsg.c_str();
		const int MASTERBROADCASTMSG_SIZE = masterBroadcastMsg.length();

		// slaves need to wait here until master reaches here
		const int numProcesses = Global::settings->NUM_PROCESSES;
		const int rank = Global::settings->RANK;
		if (rank == 0)
		{
			LOG(INFO) << "Master process [" << rank << "/" << numProcesses << "] is telling slave processes to continue...";

			// tell each slave to continue
			for (int j = 1; j < numProcesses; j++)
			{
				LOG(INFO) << "Master is sending slave process [" << j << "/" << numProcesses << "] the " << MASTERBROADCASTMSG << " message to continue...";
				int ierr = MPI_Send(const_cast<char*>(MASTERBROADCASTMSG), MASTERBROADCASTMSG_SIZE, MPI_CHAR, j, 0, MPI_COMM_WORLD);
			}

			LOG(INFO) << "Master process [" << rank << "/" << numProcesses << "] is released...";
		}
		else
		{
			LOG(INFO) << "Slave process [" << rank << "/" << numProcesses << "] is waiting for master...";

			// now wait until master gives the continue signal
			while (true)
			{
				int ierr = MPI_Recv(recvbuff, MASTERBROADCASTMSG_SIZE, MPI_CHAR, 0, 0, MPI_COMM_WORLD, Global::settings->MPI_STATUS);

				// check if correct message
				bool correctMessage = true;
				for (int i = 0; i < MASTERBROADCASTMSG_SIZE; i++)
				{
					if (recvbuff[i] != masterBroadcastMsg.at(i))
					{
						correctMessage = false;
						break;
					}
				}

				if (correctMessage)
				{
					LOG(INFO) << "Slave process [" << rank << "/" << numProcesses << "] got the " << masterBroadcastMsg << " message.";
					break;
				}
			}

			LOG(INFO) << "Slave process [" << rank << "/" << numProcesses << "] is released...";
		}
	}

	string EasyMPI::constructFullMessage(string command, string message)
	{
		// size<commandstring;messagestring>XXX...

		// sanity check
		size_t foundSemicolon1 = command.find(";");
		if (foundSemicolon1 != std::string::npos)
		{
			LOG(ERROR) << "command cannot contain a semicolon!";
			abort(1);
		}
		size_t foundSemicolon2 = message.find(";");
		if (foundSemicolon2 != std::string::npos)
		{
			LOG(ERROR) << "message cannot contain a semicolon!";
			abort(1);
		}

		// calculate size of full message
		int commandLength = command.length();
		int messageLength = message.length();
		int size = 3 + 1 + commandLength + 1 + messageLength + 1;

		if (size > MAX_MESSAGE_SIZE)
		{
			LOG(ERROR) << "Message length exceeds max message size!" << endl;
			abort(1);
		}

		// convert size to string
		stringstream sizeSS;
		sizeSS << setfill('0') << setw(3) << size;
		
		// construct full message
		stringstream ss;
		ss << sizeSS.str() << "<" << command << ";" << message << ">";
		stringstream fullMessageSS;
		fullMessageSS << std::left << setfill('X') << setw(MAX_MESSAGE_SIZE) << ss.str();
		
		//cout << "Full message constructed: '" << fullMessageSS.str() << "'" << endl;

		return fullMessageSS.str();
	}

	bool EasyMPI::parseFullMessage(string fullMessage, string& command, string& message)
	{
		// size<commandstring;messagestring>XXX...

		// get full message size
		string messageSizeString = fullMessage.substr(0, 3);
		int messageSize = atoi(messageSizeString.c_str());

		// some sanity check
		if (fullMessage.at(3) != '<' || fullMessage.at(messageSize-1) != '>')
			return false;

		// get content between < and >
		string line = fullMessage.substr(4, messageSize-5);

		// parse content to get command and message
		stringstream ss(line);
		getline(ss, command, ';');
		getline(ss, message, ';');

		//cout << "Command parsed: '" << command << "'" << endl;
		//cout << "Message parsed: '" << message << "'" << endl;

		return true;
	}
}