#include <Windows.h>
#include <iostream>
#include <stdio.h>
#include "DualModeI.h"
using namespace std;
BOOL InitializeDualMode(BOOL initMode)
{
  BOOL rc = FALSE;
  //Originally these where TCHARs, but that's not very important for what we are doing
  char szOutputPipeName[256];
  char szInputPipeName[256];
  char szErrorPipeName[256];
  char achar[4] = {'a', '\0', '\0', '\0'};
  char rchar[4] = {'r', '\0', '\0', '\0'};



  // construct named pipe names
  //
  sprintf( szOutputPipeName,
    "\\\\.\\pipe\\%dcout",
    GetCurrentProcessId() );

  sprintf( szInputPipeName,
    "\\\\.\\pipe\\%dcin",
    GetCurrentProcessId() );

  sprintf( szErrorPipeName,
    "\\\\.\\pipe\\%dcerr",
    GetCurrentProcessId() );

  // attach named pipes to stdin/stdout/stderr
  //
  rc = freopen( szOutputPipeName, &achar[0], stdout ) != NULL &&
    freopen( szInputPipeName, &rchar[0], stdin ) != NULL &&
    freopen( szErrorPipeName, &achar[0], stderr ) != NULL;


  // if unsuccessfule, i.e. no console was available
  // and initmode specifiec that we need to create one
  // we do so
  //
  //Note: you may want to comment this out based on your preferences
  if ( !rc && initMode)
  {
    rc = AllocConsole();
    if (rc)
      rc = freopen( "CONOUT$", "a", stdout ) != NULL &&
      freopen( "CONIN$", "r", stdin ) != NULL &&
      freopen( "CONERR$", "a", stderr ) != NULL;

  }

  // synchronize iostreams with standard io
  //
  if ( rc )
    ios::sync_with_stdio();

  return rc;
}
