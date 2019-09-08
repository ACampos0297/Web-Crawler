/*
	Abdul Campos
	325001471
	HW1 P1
	CSCE 463 - 500
	Fall 2019
*/

#pragma once

#include "pch.h"
#define INITIAL_BUF_SIZE 8000  //8kb
#define THRESHOLD 8000 //8kb

class Socket {
	SOCKET socket; // socket handle
	char* buf; // current buffer
	int allocatedSize; // bytes allocated for buf
	int curPos; //current position in buffer

public: 
	Socket(SOCKET sock);
	bool Read(void);
	char* getBuffer();
	int bytesReceived();
};