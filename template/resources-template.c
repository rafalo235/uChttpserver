/*
 resources-template.c

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

  Created on: Feb 28, 2018
      Author: Rafał Olejniczak
*/

#include "resources-template.h"

/*****************************************************************************/
/* Resource callbacks                                                        */
/* - of course it could be global, it's up to your needs                     */
/*****************************************************************************/
static tHttpStatusCode AaaCallback(const void *const);
static tHttpStatusCode AbaCallback(const void *const);
static tHttpStatusCode AbbCallback(const void *const);
static tHttpStatusCode AbcCallback(const void *const);
static tHttpStatusCode BbbCallback(const void *const);
static tHttpStatusCode CbbCallback(const void *const);
static tHttpStatusCode CccCallback(const void *const);
static tHttpStatusCode CeeeCallback(const void *const);
static tHttpStatusCode FaviconCallback(const void * const);
static tHttpStatusCode IndexCallback(const void * const);

/*****************************************************************************/
/* Resources table                                                           */
/* - !the resource names must be sorted! - table is looked up with binary se-*/
/*   arch, inserting resources with includes might be helpful here           */
/*****************************************************************************/
const tResourceEntry resources[] =
    {
	{ STRING_WITH_LENGTH("/aaa"), &AaaCallback },
	{ STRING_WITH_LENGTH("/aba"), &AbaCallback },
	{ STRING_WITH_LENGTH("/abb"), &AbbCallback },
	{ STRING_WITH_LENGTH("/abc"), &AbcCallback },
	{ STRING_WITH_LENGTH("/bbb"), &BbbCallback },
	{ STRING_WITH_LENGTH("/cbb"), &CbbCallback },
	{ STRING_WITH_LENGTH("/ccc"), &CccCallback },
	{ STRING_WITH_LENGTH("/ceee"), &CeeeCallback },
	{ STRING_WITH_LENGTH("/favicon"), &FaviconCallback },
	{ STRING_WITH_LENGTH("/index.html"), &IndexCallback }
    };

/*****************************************************************************/
/* Resource callback implementation                                          */
/* - use helper functions to prepare response, buffering is limited because  */
/*   of microcontroller environment usually suffers lack of memory           */
/*****************************************************************************/
static tHttpStatusCode FaviconCallback(const void * const a)
{
  return HTTP_STATUS_OK;
}

static tHttpStatusCode IndexCallback(const void * const a)
{
  /* FIXME connection struct */
  Http_HelperSendStatusLine(NULL, HTTP_STATUS_OK);
  Http_HelperSendHeaderLine(NULL, "Content-Type", "text/html");
  Http_HelperSendHeaderLine(NULL, "Connection", "close");
  Http_HelperSendCRLF(NULL);
  Http_HelperSendMessageBodyParametered(NULL,
    "<html>"
    "<head>"
    "<meta http-equiv=\"Refresh\" content=\"1\" />"
    "</head>"
    "<body>"
    "<h1>Welcome to uCHttpServer!</h1>"
    "Hello world from uCHttpServer!"
    "</body>"
    "</html>", NULL);

  return HTTP_STATUS_OK;
}

static tHttpStatusCode AaaCallback(const void *const a)
{
  return HTTP_STATUS_OK;
}

static tHttpStatusCode AbaCallback(const void *const a)
{
  return HTTP_STATUS_OK;
}

static tHttpStatusCode AbbCallback(const void *const a)
{
  return HTTP_STATUS_OK;
}

static tHttpStatusCode AbcCallback(const void *const a)
{
  return HTTP_STATUS_OK;
}

static tHttpStatusCode BbbCallback(const void *const a)
{
  return HTTP_STATUS_OK;
}

static tHttpStatusCode CbbCallback(const void *const a)
{
  return HTTP_STATUS_OK;
}

static tHttpStatusCode CccCallback(const void *const a)
{
  return HTTP_STATUS_OK;
}

static tHttpStatusCode CeeeCallback(const void *const a)
{
  return HTTP_STATUS_OK;
}
