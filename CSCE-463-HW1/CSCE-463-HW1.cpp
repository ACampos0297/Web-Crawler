/* 
	Abdul Campos
	325001471
	HW1 P3
	CSCE 463 - 500
	Fall 2019
*/

#include "pch.h"

// this class is passed to all threads, acts as shared memory
class Parameters {
public:
	//Threading
	HANDLE	mutex;
	HANDLE	finishedReading;
	HANDLE  crawlerSem;
	HANDLE	eventQuit;

	//TEMP
	Socket* recvSocket;
	char* request;
	
	//shared queue
	queue<string> urls;
	unordered_set<string> seenHosts;
	unordered_set<DWORD> seenIPs;

	string inFile;
	int extractedURLs=0;
	int passedHostUniqueness=0;
	int passedIPUniqueness = 0;
	int succDNSlook = 0;
	clock_t clock;
};

//producer thread to read file and enter URLs to queue
UINT fileReader(LPVOID pParam)
{
	Parameters* p = ((Parameters*)pParam);
	WaitForSingleObject(p->mutex, INFINITE);					// lock mutex
	// open text file
	HANDLE hFile = CreateFile(p->inFile.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	ReleaseMutex(p->mutex); //UNLOCK MUTEX

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
	
	char* pch;
	pch = strtok(fileBuf, "\r\n");
	while (pch != NULL)
	{
		WaitForSingleObject(p->mutex, INFINITE);					// lock mutex
		p->urls.push(pch);
		ReleaseSemaphore(p->crawlerSem,1,NULL);
		ReleaseMutex(p->mutex);
		pch = strtok(NULL, "\r\n");
	}

	WaitForSingleObject(p->mutex, INFINITE);
	SetEvent(p->finishedReading);
	ReleaseMutex(p->mutex);
	return 0;
}

//Stat thread
UINT statsThread(LPVOID pParam)
{
	Parameters* p = ((Parameters*)pParam);
	clock_t tcur, tlast= clock();

	while (WaitForSingleObject(p->eventQuit, 2000) == WAIT_TIMEOUT)
	{
		tcur = clock();

		WaitForSingleObject(p->mutex, INFINITE);					// lock mutex
		//https://stackoverflow.com/questions/728068/how-to-calculate-a-time-difference-in-c
		int seconds = ((double)(tlast - p->clock)) / CLOCKS_PER_SEC;

		printf(
			"[%3d] %6d Q %7d E %6d H %6d D %5d I\n",
			seconds,
			p->urls.size(),
			p->extractedURLs,
			p->passedHostUniqueness,
			p->succDNSlook,
			p->passedIPUniqueness
		);

		ReleaseMutex(p->mutex);										// unlock mutex
		tlast = tcur;
	}
	printf("Exiting stat thread");
	return 0;
}

// this function is where threadA starts
UINT crawlerThread(LPVOID pParam)
{
	Parameters* p = ((Parameters*)pParam);
	HANDLE arr[] = { p->crawlerSem, p->finishedReading };
	while (WaitForMultipleObjects(2,arr,false, INFINITE)==WAIT_OBJECT_0)
	{
		string URL;
		//perform read here ---------------------------------------------
		/*
		clock_t t = clock();
		if (p->recvSocket->Read(PAGE_SIZE))
		{
			printf("done in %.0f ms with %u bytes", (double)(clock() - t), p->recvSocket->bytesReceived());
		}
		*/

		// wait for mutex, then print and sleep inside the critical section
		WaitForSingleObject(p->mutex, INFINITE);					// lock mutex
		if (!(p->urls.size() == 0))
		{
			URL = p->urls.front(); p->urls.pop();
			(p->extractedURLs)++;
		}
		ReleaseMutex(p->mutex);						// release critical section
		//crawl here

		//Parse URL first
		//PARSING
		char* scheme;
		char* pch = new char[URL.length() + 1];
		strcpy(pch,URL.c_str());
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
		}

		if (host != "")
		{
			WaitForSingleObject(p->mutex, INFINITE);					// lock mutex
			//check host uniqueness
			int prevSize = p->seenHosts.size();
			p->seenHosts.insert(host);
			if (p->seenHosts.size() > prevSize)
			{
				//unique host add to count
				(p->passedHostUniqueness)++;
				
				//Do DNS
				// structure used in DNS lookups
				struct hostent* remote;
				// structure for connecting to server
				struct sockaddr_in server;
				// first assume that the string is an IP address
				DWORD IP = inet_addr(host.c_str());
				
				int prevIPSize = p->seenIPs.size();
				if (IP == INADDR_NONE)
				{
					// if not a valid IP, then do a DNS lookup
					if ((remote = gethostbyname(host.c_str())) == NULL)
					{
					}
					else // take the first IP address and copy into sin_addr
					{
						memcpy((char*) & (server.sin_addr), remote->h_addr, remote->h_length);
						//found IP from DNS lookup, output time and IP
						string ip = inet_ntoa(server.sin_addr);
						p->seenIPs.insert(inet_addr(ip.c_str()));
						(p->succDNSlook)++;
					}
				}
				else
				{
					// if a valid IP, directly drop its binary version into sin_addr
					server.sin_addr.S_un.S_addr = IP;
					p->seenIPs.insert(IP);
				}
				
				if (p->seenIPs.size() > prevIPSize)
				{
					(p->passedIPUniqueness)++;
					//check robots

				}
			}	
			ReleaseMutex(p->mutex);						// release critical section
		}
	}
	return 0;
}


int main(int argc, char** argv)
{
	
	//read command line arguments
	if (strcmp(argv[1], "0") == 0)
	{
		cout << "Invalid parameter" << endl;
		return 0;
	}
	
	//check input file
	if (strstr(argv[2], ".txt") == NULL)
	{
		cout << "Invalid input file" << endl;
		return 0;
	}

	//from sample code
	WSADATA wsaData;

	//Initialize WinSock; once per program run
	WORD wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		return 0;
	}

	//start crawler threads here
	int numThreads = atoi(argv[1]);

	//Prepared shared parameters based on sample code 
	Parameters p;
	// create a mutex for accessing critical sections (including printf); initial state = not locked
	p.mutex = CreateMutex(NULL, 0, NULL);
	// create a quit event for stat thread
	p.eventQuit = CreateEvent(NULL, true, false, NULL);
	//add file name
	p.inFile = string(argv[2]);
	//create event for finished reading 
	p.finishedReading = CreateEvent(NULL, true, false, NULL);
	//create semaphore for crawler threads
	p.crawlerSem = CreateSemaphore(NULL, 0, numThreads, NULL);

	HANDLE fileThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fileReader, &p, 0, NULL);

	// thread handles are stored here; they can be used to check status of threads, or kill them
	HANDLE* crawlers = new HANDLE[numThreads];

	

	HANDLE statThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)statsThread, &p, 0, NULL);
	//start n threads
	for (int i = 0; i < numThreads; i++)
		crawlers[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)crawlerThread, &p, 0, NULL);

	WaitForSingleObject(fileThread, INFINITE);
	CloseHandle(fileThread);

	//signal n threads to close
	for (int i = 0; i < numThreads; i++)
	{
		WaitForSingleObject(crawlers[i], INFINITE);
		CloseHandle(crawlers[i]);
	}

	//exit stats thread
	SetEvent(p.eventQuit);
	WaitForSingleObject(statThread, INFINITE);
	CloseHandle(statThread);
	
		

/*
					
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
													

								p.recvSocket = new Socket(sock);
								// create a mutex for accessing critical sections (including printf); initial state = not locked
								p.mutex = CreateMutex(NULL, 0, NULL);
								// create a semaphore that counts the number of active threads; initial value = 0, max = 2
								p.finished = CreateSemaphore(NULL, 0, 1, NULL);
								// create a quit event; manual reset, initial state = not signaled
								p.eventQuit = CreateEvent(NULL, true, false, NULL);

								t = clock();
								// structure p is the shared space between the threads
								for(int i=0; i<numThreads;i++)
									handles[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread, &p, 0, NULL);

								for (int i = 0; i < numThreads; i++)
								{
									printf("\n\n%.0d\n\n", numThreads);
									WaitForSingleObject(handles[i], INFINITE);
									CloseHandle(handles[i]);
								}

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
						
	*/
		
	
	// call cleanup when done with everything and ready to exit program
	WSACleanup();
	return 0;
	
}