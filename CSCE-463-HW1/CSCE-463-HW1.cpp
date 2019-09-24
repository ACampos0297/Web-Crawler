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
	int passedIPUniqueness = 0;
	int succDNSlook = 0;
	int passedRobots = 0;
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
		ReleaseMutex(p->mutex);							//release critical section
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
			"[%3d] %6d Q %7d E %6d H %6d D %5d I %5d R\n",
			seconds,
			p->urls.size(),
			p->extractedURLs,
			p->passedHostUniqueness,
			p->succDNSlook,
			p->passedIPUniqueness,
			p->passedRobots
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

	HANDLE statThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)statsThread, &p, 0, NULL);
	HANDLE fileThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fileReader, &p, 0, NULL);

	// thread handles are stored here; they can be used to check status of threads, or kill them
	HANDLE* crawlers = new HANDLE[numThreads];

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
	
	
	// call cleanup when done with everything and ready to exit program
	WSACleanup();
	return 0;
	
}