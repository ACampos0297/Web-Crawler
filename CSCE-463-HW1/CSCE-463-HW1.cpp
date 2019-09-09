/* 
	Abdul Campos
	325001471
	HW1 P1
	CSCE 463 - 500
	Fall 2019
*/

#include "pch.h"

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
				printf("\n\nURL: %s", pch);
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

						// open a TCP socket
						SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
						if (sock == INVALID_SOCKET)
						{
							printf("socket() generated error %d\n", WSAGetLastError());
							WSACleanup();
							return 0;
						}

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
								printf("Invalid string: neither FQDN, nor IP address\n");
								return 0;
							}
							else // take the first IP address and copy into sin_addr
							{
								memcpy((char*) & (server.sin_addr), remote->h_addr, remote->h_length);
								//found IP from DNS lookup, output time and IP
								printf("done in %.2f ms, found %s", (double)(clock() - t), inet_ntoa(server.sin_addr));
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
							cout << "\tConnecting on robots...";
						}
						else 
						{
							//duplicate IP
							cout << " failed\n";
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

	//perform DNS lookup ----------------------------------------
	printf("\n\tDoing DNS.. ");
	/*
	

	
	// setup the port # and protocol type
	server.sin_family = AF_INET;
	if (port != "")
	{
		server.sin_port = htons((unsigned short) stoi(port));
	}
	else
		server.sin_port = htons(80);		// host-to-network flips the byte order

	// connect to the server on port given
	printf("\n\t\b\b* Connecting on page... ");
	t = clock();
	if (connect(sock, (struct sockaddr*) & server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		printf("Connection error: %d\n", WSAGetLastError());
		return 0;
	}
	printf("done in %.0f ms", (double)(clock() - t));

	// send HTTP requests here
	printf("\n\tLoading... ");

	char getMethod[] = "GET %s HTTP/1.0\r\nUser-Agent: accrawler/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n";
	int reqBuffLen = strlen(host.c_str()) +strlen(path.c_str()) + strlen(getMethod) -4;
	char* sendBuf = new char[reqBuffLen+1];
	sprintf(sendBuf, getMethod, path.c_str(), host.c_str());
	// place request into buf
	if (send(sock, sendBuf, reqBuffLen, 0) == SOCKET_ERROR)
	{
		printf("Send error: %d\n", WSAGetLastError());
		return 0;
	}

	Socket recvSocket(sock);

	t = clock();
	recvSocket.Read();
	printf("done in %.0f ms with %u bytes\n",(double) (clock()-t), recvSocket.bytesReceived());

	// close the socket to this server; open again for the next one
	closesocket(sock);
	// call cleanup when done with everything and ready to exit program
	WSACleanup();

	printf("\tVerifying header... ");
	string header;

	string received(recvSocket.getBuffer());
	string status = received.substr(9, 3);
	cout <<"status code " <<status<<endl;
	if (stoi(status)<200||stoi(status)>299)
	{
		cout << "\n--------------------------------------------------" << endl;

		if (received.find("<html>")!= string::npos)
		{
			header = received.substr(0, received.find("<html>"));
			cout << header << endl;
		}
		else if(received.find("<head>") != string::npos)
		{
			header = received.substr(0, received.find("<head>"));
			cout << header << endl;
		}
		else if (received.find("<HTML>") != string::npos)
		{
			header = received.substr(0, received.find("<HTML>"));
			cout << header << endl;
		}
		else
		{
			header = received.substr(0, received.find("<HEAD>"));
			cout << header << endl;
		}

		return 0;
	}
	else
	{
		if (received.find("<html>") != string::npos)
		{
			header = received.substr(0, received.find("<html>"));
		}
		else if (received.find("<head>") != string::npos)
		{
			header = received.substr(0, received.find("<head>"));
		}
		else if (received.find("<HTML>") != string::npos)
		{
			header = received.substr(0, received.find("<HTML>"));
		}
		else
		{
			header = received.substr(0, received.find("<HEAD>"));
		}

		printf("\t\b\b+ Parsing page... ");
		received.erase(0, received.find("<html>"));
	}
	
	ofstream htmlOut("test.html");
	if (htmlOut.is_open())
		htmlOut << received.c_str();
	else
		return 0;

	char filename[] = "test.html";

	// open html file
	HANDLE hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	// process errors
	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf("CreateFile failed with %d\n", GetLastError());
		htmlOut.close();
		remove("test.html");
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

	// create new parser object
	HTMLParserBase* parser = new HTMLParserBase;

	host = "http://" + host;
	char baseUrl[MAX_URL_LEN];		// where this page came from; needed for construction of relative links
	strcpy(baseUrl, host.c_str());

	int nLinks;
	t = clock();
	char* linkBuffer = parser->Parse(fileBuf, fileSize, baseUrl, (int)strlen(baseUrl), &nLinks);

	// check for errors indicated by negative values
	if (nLinks < 0)
		nLinks = 0;

	printf("done in %.0f ms with %u links\n", (double) (clock()-t) ,nLinks);

	cout << "\n--------------------------------------------------" << endl;

	cout << header << endl;

	htmlOut.close();
	remove("test.html");
	delete parser;		// this internally deletes linkBuffer
	delete fileBuf;
	*/
	return 0;
}

