/*
 uchttpserver.h

 MIT License

 Copyright (c) 2018 Rafał Olejniczak

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

  Created on: Feb 23, 2018
      Author: Rafał Olejniczak
 */

#ifndef UCHTTPSERVER_H_
#define UCHTTPSERVER_H_

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef HTTP_BUFFER_LENGTH
#define HTTP_BUFFER_LENGTH (256)
#endif

#ifndef HTTP_PARAMETERS_BUFFER_LENGTH
#define HTTP_PARAMETERS_BUFFER_LENGTH (640)
#endif

#ifndef HTTP_PARAMETERS_MAX
#define HTTP_PARAMETERS_MAX (16)
#endif

typedef enum HttpStatusCode
{
  HTTP_STATUS_OK,
  HTTP_STATUS_CONTINUE,
  HTTP_BAD_REQUEST,
  HTTP_FORBIDDEN,
  HTTP_STATUS_NOT_FOUND,
  HTTP_STATUS_REQUEST_URI_TOO_LONG,
  HTTP_STATUS_SERVER_FAULT,
  HTTP_STATUS_NOT_IMPLEMENTED
} tHttpStatusCode;

typedef enum HttpMethod
{
  HTTP_CONNECT = 0,
  HTTP_DELETE,
  HTTP_GET,
  HTTP_HEAD,
  HTTP_OPTIONS,
  HTTP_POST,
  HTTP_PUT,
  HTTP_TRACE,
} tHttpMethod;

/*****************************************************************************/
/* Common                                                                    */
/*****************************************************************************/

#define STRING_WITH_LENGTH(x) {x, sizeof(x) - 1}

typedef struct StringWithLength
{
  const char *str;
  unsigned int length;
} tStringWithLength;

/*****************************************************************************/
/* Resources                                                                 */
/*****************************************************************************/

typedef tHttpStatusCode (*tResourceCallback)(void * const);

typedef struct ResourceEntry
{
  tStringWithLength name;
  tResourceCallback callback;
} tResourceEntry;

/*****************************************************************************/
/* Search entity                                                             */
/*****************************************************************************/

typedef const tStringWithLength *
    (*tGetElementByIdxCallback)(const void *, unsigned int);

typedef struct SearchEntity
{
  const void * array;
  char * buffer;
  unsigned int length;
  unsigned int left;
  unsigned int right;
  tGetElementByIdxCallback getElementByIdx;
  unsigned char compareIdx;
  unsigned char bufferIdx;
  unsigned char bufferLength;
} tSearchEntity;

/*****************************************************************************/
/* Compare entity                                                            */
/*****************************************************************************/

typedef struct CompareEntity
{
  unsigned char compareIdx;
} tCompareEntity;

/*****************************************************************************/
/* Error information                                                         */
/*****************************************************************************/

typedef struct ErrorInfo
{
  tHttpStatusCode status;
} tErrorInfo;

/*****************************************************************************/
/* General inteface                                                          */
/*****************************************************************************/

typedef unsigned int (*tSendCallback)(
    void * const conn, const char * data, unsigned int length);

typedef void (*tErrorCallback)(
    void * const conn, const tErrorInfo * errorInfo);

typedef unsigned int (*tParserState)(void * const,
    const char * data, unsigned int length);

typedef struct SearchPhaseArea
{
  tSearchEntity searchEntity;

} tSearchPhaseArea;

typedef struct ParsePhaseArea
{
  tCompareEntity compareEntity;

} tParsePhaseArea;

typedef union SharedArea
{
  tSearchPhaseArea search;
  tParsePhaseArea parse;
  tErrorInfo errorInfo;
} tSharedArea;

typedef struct uCHttpServerState
{
  tSharedArea shared;
  unsigned char method;
  unsigned char compareIdx; /* resource path limit */
  unsigned char byte;
  unsigned int resourceIdx;
  unsigned int left;
  unsigned int right;
  unsigned int contentLength;
  tParserState state;
  const tResourceEntry (*resources)[]; /* Or set as singleton */
  unsigned int resourcesLength;
  tSendCallback send;
  tErrorCallback onError;
  void * context;
  unsigned int bufferIdx;
  char buffer[HTTP_BUFFER_LENGTH];
  char parametersBuffer[HTTP_PARAMETERS_BUFFER_LENGTH];
  char * parameters[HTTP_PARAMETERS_MAX][2];
} tuCHttpServerState;

/*****************************************************************************/
/* Integration API                                                           */
/*****************************************************************************/

/**
 * \brief Initialize connection state machine
 * Must be called once per connection before any
 * processing
 */
void
Http_InitializeConnection(tuCHttpServerState * const sm,
			  tSendCallback send,
			  tErrorCallback onError,
			  const tResourceEntry (*resources)[],
			  unsigned int reslen,
			  void * context);

/**
 * \brief Entry point for input stream processing
 */
void
Http_Input(tuCHttpServerState * const sm,
	   const char * data, unsigned int length);

/*****************************************************************************/
/* Helper API                                                                */
/*****************************************************************************/

tHttpMethod Http_HelperGetMethod(tuCHttpServerState * const sm);

void * Http_HelperGetContext(tuCHttpServerState * const sm);

const char * Http_HelperGetParameter(
    tuCHttpServerState * const sm, const char * param);

void Http_HelperSendStatusLine(
    tuCHttpServerState * const sm, tHttpStatusCode code);

void Http_HelperSendHeaderLine(
    tuCHttpServerState * const sm, const char * name, const char * value);

void Http_HelperSendCRLF(tuCHttpServerState * const sm);

void Http_HelperSendMessageBody(
    tuCHttpServerState * const sm, const char * body);

void Http_HelperSendMessageBodyParametered(
    tuCHttpServerState * const sm,
    const char * body, const void * const * param);

void Http_HelperSend(
    tuCHttpServerState * const sm, const char * data, unsigned int length);

void Http_HelperSendParametered(
    tuCHttpServerState * const sm, const char * data, unsigned int length,
    const void * const * param);

void Http_HelperFlush(tuCHttpServerState * const sm);

#endif /* UCHTTPSERVER_H_ */
