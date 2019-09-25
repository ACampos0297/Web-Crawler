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
	
	//shared queue
	queue<string> urls;
	unordered_set<string> seenHosts;
	unordered_set<DWORD> seenIPs;

	string inFile;
	int extractedURLs=0;
	int passedHostUniqueness=0;
	int numActiveThreads = 0;
	int passedIPUniqueness = 0;
	int succDNSlook = 0;
	int passedRobots = 0;

	int validHTTP = 0; //numURLS
	int bytesDownloaded = 0;
	int totalBytesDownloaded = 0;
	int totalCrawled = 0;
	int totalParsed = 0;

	int x200Codes = 0;
	int x300Codes = 0;
	int x400Codes = 0;
	int x500Codes = 0;
	int otherCodes = 0;

	int numLinks = 0;
	clock_t clock;
};


//Stat thread
UINT statsThread(LPVOID pParam)
{
	Parameters* p = ((Parameters*)pParam);
	clock_t tcur, tlast= clock();
	int seconds = 0;
	while (WaitForSingleObject(p->eventQuit, 2000) == WAIT_TIMEOUT)
	{
		//https://stackoverflow.com/questions/728068/how-to-calculate-a-time-difference-in-c
		tcur = clock();
		double sinceLast = ((double)(tcur-tlast)) / CLOCKS_PER_SEC;
		
		WaitForSingleObject(p->mutex, INFINITE);					// lock mutex
		
		seconds = ((double)(tlast - p->clock)) / CLOCKS_PER_SEC;

		printf(
			"[%3d] %5d Q %6d E %7d H %6d D %6d I %5d R %5d C %5d L %4dK \n",
			seconds,
			p->numActiveThreads,
			p->urls.size(),
			p->extractedURLs,
			p->passedHostUniqueness,
			p->succDNSlook,
			p->passedIPUniqueness,
			p->passedRobots,
			p->totalCrawled,
			p->numLinks/1000
		);

		printf("\t*** crawling %.2f pps @%.1f Mbps\n", 
			(float)(p->validHTTP/sinceLast),
			(float)((p->bytesDownloaded/1000) / sinceLast)
		);

		p->totalBytesDownloaded += p->bytesDownloaded;
		p->totalCrawled += p->validHTTP;

		p->bytesDownloaded = 0;
		p->validHTTP=0;

		ReleaseMutex(p->mutex);										// unlock mutex
		tlast = tcur;
	}
	
	
	printf("Extracted %7d URLS @%.0f/s\n"
		"Looked up %6d DNS names @%.0f/s\n"
		"Downloaded %5d robots @%.0f/s\n"
		"Crawled %5d pages @%.0f/s (%.1f MB)\n"
		"Parsed %7d links @%.0f/s\n"
		"HTTP codes: 2xx = %6d, 3xx = %6d, 4xx = %6d, 5xx = %6d, other = %6d",
		p->extractedURLs, (float)(p->extractedURLs/seconds),
		p->succDNSlook, (float)(p->succDNSlook/seconds),
		p->passedRobots, (float)(p->passedRobots/seconds),
		p->totalCrawled, (float)(p->totalCrawled/seconds), (float)(p->totalBytesDownloaded/1000),
		p->totalParsed, (float)(p->totalParsed/seconds),
		p->x200Codes, p->x300Codes, p->x400Codes, p->x500Codes, p->otherCodes
	);

	return 0;
}

// this function is where threadA starts
UINT crawlerThread(LPVOID pParam)
{
	Parameters* p = ((Parameters*)pParam);
	HTMLParserBase* parser = new HTMLParserBase;
	WaitForSingleObject(p->mutex, INFINITE);					// lock mutex
	(p->numActiveThreads)++;
	ReleaseMutex(p->mutex);						// release critical section
	while (true)
	{
		string URL;

		// wait for mutex, then print and sleep inside the critical section
		WaitForSingleObject(p->mutex, INFINITE);					// lock mutex
		if (p->totalCrawled > 10000)
			break;
		if(p->urls.size()==0)
		{
			break;
			ReleaseMutex(p->mutex);						// release critical section	
		}
		URL = p->urls.front(); p->urls.pop();
		(p->extractedURLs)++;
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
			// structure used in DNS lookups
			struct hostent* remote;
			// structure for connecting to server
			struct sockaddr_in server;
			// first assume that the string is an IP address
			DWORD IP = inet_addr(host.c_str());
			
			//check host uniqueness
			WaitForSingleObject(p->mutex, INFINITE);					// lock mutex
			int prevSize = p->seenHosts.size();
			p->seenHosts.insert(host);
			
			if (p->seenHosts.size() > prevSize)
			{
				//unique host 
				(p->passedHostUniqueness)++; //increase passed host count
				ReleaseMutex(p->mutex);						// release critical section

				bool failedDNS = false;
				//do DNS lookup
				int prevIPSize = 0;
				if (IP == INADDR_NONE)
				{
					// if not a valid IP, then do a DNS lookup
					if ((remote = gethostbyname(host.c_str())) == NULL)
					{
						failedDNS = true;
					}
					else // take the first IP address and copy into sin_addr
					{
						memcpy((char*) & (server.sin_addr), remote->h_addr, remote->h_length);
						//found IP from DNS lookup, output time and IP
						WaitForSingleObject(p->mutex, INFINITE);					// lock mutex
						(p->succDNSlook)++; //increase count of successfull DNS lookups
						ReleaseMutex(p->mutex);						// release critical section
					}
				}
				else
				{
					// if a valid IP, directly drop its binary version into sin_addr
					server.sin_addr.S_un.S_addr = IP;
				}
				
				WaitForSingleObject(p->mutex, INFINITE);					// lock mutex
				string ip = "";
				if(!failedDNS)
					ip = inet_ntoa(server.sin_addr);
				prevIPSize = p->seenIPs.size();
				p->seenIPs.insert(inet_addr(ip.c_str()));
				if (p->seenIPs.size() > prevIPSize)
				{
					//unique IP
					(p->passedIPUniqueness)++;
					ReleaseMutex(p->mutex);						// release critical section
					
					//connect on robots
					// open a TCP socket
					SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
					if (sock == INVALID_SOCKET)
					{
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

						if (connect(sock, (struct sockaddr*) & server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
						{
						}
						else
						{
							
							// send HTTP robot requests here
							char headMethod[] = "HEAD %s HTTP/1.0\r\nUser-Agent: accrawler/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n";
							string robpath = "/robots.txt";
							int reqBuffLen = strlen(host.c_str()) + strlen(robpath.c_str()) + strlen(headMethod) - 4;
							char* sendBuf = new char[reqBuffLen + 1];

							sprintf(sendBuf, headMethod, robpath.c_str(), host.c_str());

							// place request into buf
							if (send(sock, sendBuf, reqBuffLen, 0) == SOCKET_ERROR)
							{
							}
							else
							{
								delete[] sendBuf;
								Socket recvSocket(sock);
								if (recvSocket.Read(ROBOT_SIZE))
								{
									// close the socket to this server; open again for the next one
									closesocket(sock);

									string header;

									string received(recvSocket.getBuffer());
									string status = "";
									if(received.size()>8)
										status = received.substr(9, 3);

									if (status[0] == '4')
									{
										//robot exists, download page
										WaitForSingleObject(p->mutex, INFINITE);					// lock mutex
										(p->passedRobots)++;
										(p->bytesDownloaded) = (p->bytesDownloaded) + recvSocket.bytesReceived();
										ReleaseMutex(p->mutex);						// release critical section
										//robot exists, download page
										// open a TCP socket
										SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

										if (sock == INVALID_SOCKET)
										{
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

											if (connect(sock, (struct sockaddr*) & server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
											{
											}
											else
											{
												
												// send HTTP requests here
												char getMethod[] = "GET %s HTTP/1.0\r\nUser-Agent: accrawler/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n";
												int reqBuffLen = strlen(host.c_str()) + strlen(path.c_str()) + strlen(getMethod) - 4;
												char* sendBuf = new char[reqBuffLen + 1];

												sprintf(sendBuf, getMethod, path.c_str(), host.c_str());
												
												// place request into buf
												if (send(sock, sendBuf, reqBuffLen, 0) == SOCKET_ERROR)
												{
												}
												else
												{
													delete[] sendBuf;
													Socket recvSocket(sock);
													if (recvSocket.Read(PAGE_SIZE))
													{
														closesocket(sock);

														string header;

														string received(recvSocket.getBuffer());
														string status = "";
														if(received.size()>8)
															status = received.substr(9, 3);
														
														WaitForSingleObject(p->mutex, INFINITE);					// lock mutex
														switch (status[0])
														{
														case '2': (p->x200Codes)++;
															break;
														case '3': (p->x300Codes)++;
															break;
														case '4': (p->x400Codes)++;
															break;
														case '5': (p->x500Codes)++;
															break;
														default: (p->otherCodes)++;
															break;
														}
														ReleaseMutex(p->mutex);						// release critical section

														if (status[0] == '2')
														{
															//remove HEAD info 
															received.erase(0, received.find("<"));
															int nLinks=0;
															host = "http://" + host;
															parser->Parse((char*)received.c_str(), received.length(), (char*)host.c_str(), (int)strlen(host.c_str()), &nLinks);
															WaitForSingleObject(p->mutex, INFINITE);					// lock mutex
															p->bytesDownloaded += recvSocket.bytesReceived();
															(p->validHTTP)++;
															if(nLinks>0)
															{
																p->numLinks += nLinks;
															}
															ReleaseMutex(p->mutex);						// release critical section
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
					ReleaseMutex(p->mutex);						// release critical section
				}
				
			}
			else
			{
				//failed host uniqueness 
				ReleaseMutex(p->mutex);						// release critical section
			}
			

		}
		
	}
	WaitForSingleObject(p->mutex, INFINITE);					// lock mutex
	(p->numActiveThreads)--;
	ReleaseMutex(p->mutex);						// release critical section
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


	// open text file
	HANDLE hFile = CreateFile(p.inFile.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
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

	char* pch;
	pch = strtok(fileBuf, "\r\n");
	while (pch != NULL)
	{
		p.urls.push(pch);
		pch = strtok(NULL, "\r\n");
	}
	p.clock = clock();

	HANDLE statThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)statsThread, &p, 0, NULL);

	// thread handles are stored here; they can be used to check status of threads, or kill them
	HANDLE* crawlers = new HANDLE[numThreads];

	//start n threads
	for (int i = 0; i < numThreads; i++)
		crawlers[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)crawlerThread, &p, 0, NULL);

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
	
	
	// call cleanup when done with everything and ready to exit program
	WSACleanup();
	return 0;
	
}