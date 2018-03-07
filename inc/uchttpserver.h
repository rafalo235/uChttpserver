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

#define STRING_WITH_LENGTH(x) {x, sizeof(x) - 1}

typedef enum HttpStatusCode
{
  HTTP_STATUS_OK,
  HTTP_STATUS_CONTINUE,
  HTTP_STATUS_NOT_FOUND
} tHttpStatusCode;

typedef enum HttpMethod
{
  HTTP_GET = 0,
  HTTP_POST,
  HTTP_OPTIONS,
  HTTP_HEAD,
  HTTP_PUT,
  HTTP_DELETE,
  HTTP_TRACE,
  HTTP_CONNECT
} tHttpMethod;

typedef struct StringWithLength
{
  const char *str;
  unsigned int length;
} tStringWithLength;

typedef tHttpStatusCode (*tResourceCallback)(const void * const);

typedef unsigned int (*tParserState)(void * const,
    const char * data, unsigned int length);
typedef int (*tCompareFunction)(
    const void * a, const void * b, unsigned int length);

typedef struct ResourceEntry
{
  tStringWithLength name;
  tResourceCallback callback;
} tResourceEntry;

typedef struct uCHttpServerState
{
  unsigned char method;
  unsigned char compareIdx; /* resource path limit */
  unsigned char byte;
  unsigned int resourceIdx;
  unsigned int left;
  unsigned int right;
  tParserState state;
  const tResourceEntry (*resources)[]; /* Or set as singleton */
  unsigned int resourcesLength;
} tuCHttpServerState;

/* TODO configurable port name */
void
Http_SendPort(const char * data, unsigned int length);

/**
 * \brief Initialize connection state machine
 * Must be called once per connection before any
 * processing
 */
void
Http_InitializeConnection(tuCHttpServerState * const sm,
			  const tResourceEntry (*resources)[],
			  unsigned int reslen);

/**
 * \brief Entry point for input stream processing
 */
void
Http_Input(tuCHttpServerState * const sm,
	   const char * data, unsigned int length);

tHttpMethod Http_HelperGetMethod(tuCHttpServerState * const sm);
void Http_HelperSendStatusLine(
    tuCHttpServerState * const sm, tHttpStatusCode code);
void Http_HelperSendHeaderLine(
    tuCHttpServerState * const sm, const char * name, const char * value);
void Http_HelperSendCRLF(tuCHttpServerState * const sm);
void Http_HelperSendMessageBody(
    tuCHttpServerState * const sm, const char * body);

#endif /* UCHTTPSERVER_H_ */
