/*
	Abdul Campos
	325001471
	HW1 P1
	CSCE 463 - 500
	Fall 2019
*/

#include "Socket.h"

Socket::Socket(SOCKET sockToRead)
{
	// allocate this buffer once, then possibly reuse for multiple connections in Part 3
	buf = new char[INITIAL_BUF_SIZE]; // start with 8 KB
	socket = sockToRead;
	allocatedSize = INITIAL_BUF_SIZE;
	curPos = 0;
}

bool Socket::Read()
{
	// set timeout to 10 seconds
	struct timeval tv;
	int ret;

	//from stack overflow
	fd_set fds; 
	FD_ZERO(&fds);
	FD_SET(socket, &fds);

	tv.tv_sec=10;
	tv.tv_usec=0;

	while (true)
	{
		// wait to see if socket has any data (see MSDN)
		if ((ret = select(socket, &fds, NULL, NULL, &tv)) > 0)
		{
			// new data available; now read the next segment
			int bytes = recv(socket, buf + curPos, allocatedSize- curPos, 0);
			if (bytes < 0)
			{
				cout << " " << WSAGetLastError() << endl;
				break;
			}
			if (bytes == 0)
			{
				buf[curPos] = 0;
				return true; // normal completion
			}
			curPos += bytes; // adjust where the next recv goes
			if ((allocatedSize - curPos) < THRESHOLD)
			{
				allocatedSize *= 2;
				//from stack overflow https://stackoverflow.com/questions/1401234/differences-between-using-realloc-vs-free-malloc-functions
				char* newBuf = (char*)realloc(buf, allocatedSize);
				buf = newBuf;
				if (newBuf == 0)
				{
					free(buf);
				}
			}
		}
		else if (ret==0)
		{
			cout << " connection timed out" << endl;
			break;
		}
		else
		{
			cout << " " << WSAGetLastError()<<endl;
			break;
		}
	}
	return false;
}

char* Socket::getBuffer()
{
	return this->buf;
}

int Socket :: bytesReceived()
{
	return curPos;
}