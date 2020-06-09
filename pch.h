/*
	Abdul Campos
	325001471
	HW1 P1
	CSCE 463 - 500
	Fall 2019
*/

// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#pragma once
#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable:4996)
#pragma warning(disable:4267)
#pragma warning(disable:4244)
#pragma warning(disable:4477)


#ifndef PCH_H
#define PCH_H

#define ROBOT_SIZE 16000 //16kb for robot
#define PAGE_SIZE 2000000 //2MB for pages
// add headers that you want to pre-compile here

#include <stdio.h>
#include <windows.h>
#include <string>
#include <iostream>
#include <ctime>
#include <fstream>
#include <unordered_set>
#include <queue>

#include "HTMLParserBase.h"
#include "Socket.h"

using namespace std;
#endif //PCH_H
