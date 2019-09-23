/* 
	Abdul Campos
	325001471
	HW1 P2
	CSCE 463 - 500
	Fall 2019
*/

#include "pch.h"

// this class is passed to all threads, acts as shared memory
class Parameters {
public:
	HANDLE	mutex;
	HANDLE	finished;
	HANDLE	eventQuit;
	Socket* recvSocket;
	char* request;
};

// this function is where threadA starts
UINT threadA(LPVOID pParam)
{
	Parameters* p = ((Parameters*)pParam);

	// wait for mutex, then print and sleep inside the critical section
	WaitForSingleObject(p->mutex, INFINITE);					// lock mutex
	//perform read here ---------------------------------------------
	clock_t t = clock();
	if (p->recvSocket->Read(PAGE_SIZE))
	{
		printf("done in %.0f ms with %u bytes", (double)(clock() - t), p->recvSocket->bytesReceived());
	}

	ReleaseMutex(p->mutex);									// release critical section

	// signal that this thread has finished its job
	ReleaseSemaphore(p->finished, 1, NULL);

	// wait for threadB to allow us to quit
	//WaitForSingleObject(p->eventQuit, INFINITE);

	// print we're about to exit
	WaitForSingleObject(p->mutex, INFINITE);
	//printf("threadA %d quitting on event\n", GetCurrentThreadId());
	ReleaseMutex(p->mutex);

	return 0;
}

// this function is where threadB starts
UINT threadB(LPVOID pParam)
{
	Parameters* p = ((Parameters*)pParam);

	// wait for both threadA threads to quit
	WaitForSingleObject(p->finished, INFINITE);
	WaitForSingleObject(p->finished, INFINITE);

	printf("threadB woken up!\n");				// no need to sync as only threadB can print at this time
	Sleep(3000);

	printf("threadB setting eventQuit\n"); 	// no need to sync as only threadB can print at this time

	// force other threads to quit
	SetEvent(p->eventQuit);

	return 0;
}

int main(int argc, char** argv)
{
	//read command line arguments
	if (strcmp(argv[1],"1")!=0)
	{
		cout << "Invalid parameter" << endl;
		return 0;
	}
	else
	{
		//check input file
		if (strstr(argv[2],".txt") == NULL)
		{
			cout << "Invalid input file" << endl;
			return 0;
		}
		else
		{
			// open text file
			HANDLE hFile = CreateFile(argv[2], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL, NULL);
			// process errors
			if (hFile == INVALID_HANDLE_VALUE)
			{
				printf("Unable to open file\n");
				return 0;
			}

			// get file size
			LARGE_INTEGER li;
			BOOL bRet = GetFileSizeEx(hFile, &li);
			// process errors
			if (bRet == 0)
			{
				printf("GetFileSizeEx error %d\n", GetLastError());
				return 0;
			}

			// read file into a buffer
			int fileSize = (DWORD)li.QuadPart;			// assumes file size is below 2GB; otherwise, an __int64 is needed
			DWORD bytesRead;
			// allocate buffer
			char* fileBuf = new char[fileSize];
			// read into the buffer
			bRet = ReadFile(hFile, fileBuf, fileSize, &bytesRead, NULL);
			// process errors
			if (bRet == 0 || bytesRead != fileSize)
			{
				printf("ReadFile failed with %d\n", GetLastError());
				return 0;
			}

			// done with the file
			CloseHandle(hFile);

			printf("Opened %s with size %u", argv[2], li);

			//from sample code
			WSADATA wsaData;

			//Initialize WinSock; once per program run
			WORD wVersionRequested = MAKEWORD(2, 2);
			if (WSAStartup(wVersionRequested, &wsaData) != 0) {
				printf("WSAStartup error %d\n", WSAGetLastError());
				WSACleanup();
				return 0;
			}

			unordered_set<string> seenHosts;
			unordered_set<DWORD> seenIPs;

			char* pch;
			pch = strtok(fileBuf, "\n");
			while (pch != NULL)
			{
				printf("\nURL: %s", pch);
				printf("\n\tParsing URL... ");
				char* scheme;
				scheme = strstr(pch, "http://");

				string url, query, path, port, host;

				//retreive url, query, path, port, host from command line argument 
				if (scheme != NULL)
				{
					//remove scheme
					url = string(pch);
					pch = pch + 7;

					//remove fragment
					char* frag = strchr(pch, '#');
					if (frag != NULL)
						pch[strlen(pch) - strlen(frag)] = 0;

					//get query
					char* qry = strchr(pch, '?');
					if (qry != NULL)
					{
						qry++;
						query = string(qry);
						pch[strlen(pch) - strlen(qry) - 1] = 0;
					}

					//get path
					char* pth = strchr(pch, '/');
					if (pth != NULL)
					{
						path = string(pth);
						pch[strlen(pch) - strlen(pth)] = 0;
					}
					else
						path = "/";

					//get port
					char* pt = strchr(pch, ':');
					if (pt != NULL)
					{
						pt++;
						if (strlen(pt) < 1 || strlen(pt) > 5)
						{
							cout << "failed with invalid port\n";
							return 0;
						}

						port = string(pt);
						pch[strlen(pch) - strlen(pt) - 1] = 0;
					}
					
					//get host
					if (pch != NULL && strlen(pch) > 0)
						host = string(pch);

					if (host != "")
						cout << "host " << host;
					if (port != "")
						cout << ", port " << port;
					if (port == "")
						cout << ", port 80";
					cout << endl;
				}
				else
				{
					printf("failed with invalid scheme\n");
				}
				
if (host != "")
{
	//check host uniqueness
	printf("\tChecking host uniqueness... ");
	int prevSize = seenHosts.size();
	seenHosts.insert(host);
	if (seenHosts.size() > prevSize)
	{
		//unique host
		cout << "passed" << endl;
		cout << "\tDoing DNS... ";
		//start clock 
		clock_t t = clock();

		// structure used in DNS lookups
		struct hostent* remote;
		// structure for connecting to server
		struct sockaddr_in server;
		// first assume that the string is an IP address
		DWORD IP = inet_addr(host.c_str());

		int prevIPSize = seenIPs.size();

		if (IP == INADDR_NONE)
		{
			// if not a valid IP, then do a DNS lookup
			if ((remote = gethostbyname(host.c_str())) == NULL)
			{
				printf("Invalid string: neither FQDN, nor IP address");
			}
			else // take the first IP address and copy into sin_addr
			{
				memcpy((char*) & (server.sin_addr), remote->h_addr, remote->h_length);
				//found IP from DNS lookup, output time and IP
				printf("done in %.0f ms, found %s", (double)(clock() - t), inet_ntoa(server.sin_addr));
				string ip = inet_ntoa(server.sin_addr);
				seenIPs.insert(inet_addr(ip.c_str()));
			}
		}
		else
		{
			printf("done in 0 ms, found %s", host.c_str());
			// if a valid IP, directly drop its binary version into sin_addr
			server.sin_addr.S_un.S_addr = IP;
			seenIPs.insert(IP);
		}

		cout << "\n\tChecking IP Uniqueness...";
		if (seenIPs.size() > prevIPSize)
		{
			cout << " passed\n";
			//unique 
			//Connect on robots
			cout << "\tConnecting on robots...";
							
							
							
			// open a TCP socket
			SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (sock == INVALID_SOCKET)
			{
				printf("socket() generated error %d\n", WSAGetLastError());
			}
			else
			{
				// setup the port # and protocol type
				server.sin_family = AF_INET;
				if (port != "")
				{
					server.sin_port = htons((unsigned short)stoi(port));
				}
				else
					server.sin_port = htons(80);		// host-to-network flips the byte order

				// connect to the server on port given
				t = clock();
				if (connect(sock, (struct sockaddr*) & server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
				{
					printf("Connection error: %d", WSAGetLastError());
				}
				else
				{
					printf("done in %.0f ms", (double)(clock() - t));
								
					// send HTTP requests here
					printf("\n\tLoading...");
;
					path = path.substr(0,path.length()-1);
					char headMethod[] = "HEAD %s HTTP/1.0\r\nUser-Agent: accrawler/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n";
					int headreqBuffLen = strlen(host.c_str()) + strlen(path.c_str()) + strlen(headMethod);
					char* sendBuf = new char[headreqBuffLen + 1];

					sprintf(sendBuf, "HEAD %s HTTP/1.0\r\nUser-Agent: accrawler/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path.c_str(), host.c_str());
					
					// place request into buf
					if (send(sock, sendBuf, headreqBuffLen, 0) == SOCKET_ERROR)
					{
						printf("Send error: %d\n", WSAGetLastError());
					}
					else
					{
						Socket recvSocket(sock);
						clock_t t = clock();
						if (recvSocket.Read(ROBOT_SIZE))
						{
							printf("done in %.0f ms with %u bytes", (double)(clock() - t), recvSocket.bytesReceived());
							// close the socket to this server; open again for the next one
							closesocket(sock);

							printf("\n\tVerifying header... ");
							string header;

							string received(recvSocket.getBuffer());
							string status = received.length()>0?received.substr(9, 3):"200";
							cout << "status code " << status;

	if (status[0] == '4')
	{
		//robot exists, download page
		cout << "\n\t\b\b* Connecting on page...";
		// open a TCP socket
		SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == INVALID_SOCKET)
		{
			printf("socket() generated error %d\n", WSAGetLastError());
		}
		else
		{
			// setup the port # and protocol type
			server.sin_family = AF_INET;
			if (port != "")
			{
				server.sin_port = htons((unsigned short)stoi(port));
			}
			else
				server.sin_port = htons(80);		// host-to-network flips the byte order

			// connect to the server on port given
			t = clock();
			if (connect(sock, (struct sockaddr*) & server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
			{
				printf("Connection error: %d", WSAGetLastError());
			}
			else
			{
				printf("done in %.0f ms", (double)(clock() - t));

				// send HTTP requests here
				printf("\n\tLoading... ");

				char getMethod[] = "GET %s HTTP/1.0\r\nUser-Agent: accrawler/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n";
				int reqBuffLen = strlen(host.c_str()) + strlen(path.c_str()) + strlen(getMethod) - 4;
				char* sendBuf = new char[reqBuffLen + 1];
				
				sprintf(sendBuf, getMethod, path.c_str(), host.c_str());
				
				if (send(sock, sendBuf, reqBuffLen, 0) == SOCKET_ERROR)
				{
					printf("Send error: %d\n", WSAGetLastError());
				}
				else
				{
					// thread handles are stored here; they can be used to check status of threads, or kill them
					HANDLE* handles = new HANDLE[1];

					Parameters p;
					p.recvSocket = new Socket(sock);
					// create a mutex for accessing critical sections (including printf); initial state = not locked
					p.mutex = CreateMutex(NULL, 0, NULL);
					// create a semaphore that counts the number of active threads; initial value = 0, max = 2
					p.finished = CreateSemaphore(NULL, 0, 1, NULL);
					// create a quit event; manual reset, initial state = not signaled
					p.eventQuit = CreateEvent(NULL, true, false, NULL);

					t = clock();
					// structure p is the shared space between the threads
					handles[0] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)threadA, &p, 0, NULL);


					WaitForSingleObject(handles[0], INFINITE);
					CloseHandle(handles[0]);

					// close the socket to this server; open again for the next one
					closesocket(sock);

					printf("\n\tVerifying header... ");

					string received(p.recvSocket->getBuffer());
					string recstatus = received.substr(9, 3);
					cout << "status code " << recstatus;

					if (recstatus[0] == '2')
					{
						printf("\t\b\b+ Parsing page... ");
						received.erase(0, received.find("<html>"));
						ofstream htmlOut("test.html");

						if (htmlOut.is_open())
						{
							htmlOut << received.c_str();
							char filename[] = "test.html";
							// open html file
							HANDLE hFile = CreateFile(filename, GENERIC_READ, 
								FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
								FILE_ATTRIBUTE_NORMAL, NULL);
							// process errors
							if (hFile == INVALID_HANDLE_VALUE)
							{
								printf("CreateFile failed with %d\n", GetLastError());
								htmlOut.close();
								remove("test.html");
							}
							else {
								// get file size
								LARGE_INTEGER li;
								BOOL bRet = GetFileSizeEx(hFile, &li);
								// process errors
								if (bRet == 0)
								{
									printf("GetFileSizeEx error %d\n", GetLastError());
								}
								else {
								// read file into a buffer
								int fileSize = (DWORD)li.QuadPart;
								DWORD bytesRead;
								// allocate buffer
								char* fileBuf = new char[fileSize];
								// read into the buffer
								bRet = ReadFile(hFile, fileBuf, fileSize, &bytesRead, NULL);
								// process errors
								if (bRet == 0 || bytesRead != fileSize)
								{
									printf("ReadFile failed with %d\n", GetLastError());
								}
								else {
									// done with the file
									CloseHandle(hFile);
									// create new parser object
									HTMLParserBase* parser = new HTMLParserBase;
									host = "http://" + host;
									// where this page came from; needed for construction of relative links
									char baseUrl[MAX_URL_LEN];
									strcpy(baseUrl, host.c_str());
									int nLinks;
									t = clock();
									char* linkBuffer = parser->Parse(fileBuf, fileSize, baseUrl, (int)strlen(baseUrl), &nLinks);
									// check for errors indicated by negative values
									if (nLinks < 0)
										nLinks = 0;
									printf("done in %.0f ms with %u links\n", (double)(clock() - t), nLinks);
									cout << "\n--------------------------------------------------" << endl;
									cout << header << endl;
									htmlOut.close();
									remove("test.html");
									delete parser;		// this internally deletes linkBuffer
									delete fileBuf;
								}
								}
							}
						}
						else 
						{
							cout << "Unable to open file";
						}
					}
				}
			}
		}
	}
						}
					}
				}
			}
		}
		else 
		{
			//duplicate IP
			cout << " failed";
		}
						
	}
	else
	{
		//duplicate so exit
		cout << "failed";
	}
}
host = port = path = query = "";
pch = strtok(NULL, "\n");
			}

		}
	}
	// call cleanup when done with everything and ready to exit program
	WSACleanup();
	return 0;
}

