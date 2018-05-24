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
static tHttpStatusCode AaaCallback(
    void *const);
static tHttpStatusCode AbaCallback(
    void *const);
static tHttpStatusCode AbbCallback(
    void *const);
static tHttpStatusCode AbcCallback(
    void *const);
static tHttpStatusCode BbbCallback(
    void *const);
static tHttpStatusCode CbbCallback(
    void *const);
static tHttpStatusCode CccCallback(
    void *const);
static tHttpStatusCode CeeeCallback(
    void *const);
static tHttpStatusCode FaviconCallback(
    void *const);
static tHttpStatusCode IndexCallback(
    void *const);

/*****************************************************************************/
/* Resources table                                                           */
/* - !the resource names must be sorted! - table is looked up with binary se-*/
/*   arch, inserting resources with includes might be helpful here           */
/*****************************************************************************/
const tResourceEntry resources[] = {
  {STRING_WITH_LENGTH("/aaa"), &AaaCallback},
  {STRING_WITH_LENGTH("/aba"), &AbaCallback},
  {STRING_WITH_LENGTH("/abb"), &AbbCallback},
  {STRING_WITH_LENGTH("/abc"), &AbcCallback},
  {STRING_WITH_LENGTH("/bbb"), &BbbCallback},
  {STRING_WITH_LENGTH("/cbb"), &CbbCallback},
  {STRING_WITH_LENGTH("/ccc"), &CccCallback},
  {STRING_WITH_LENGTH("/ceee"), &CeeeCallback},
  {STRING_WITH_LENGTH("/favicon"), &FaviconCallback},
  {STRING_WITH_LENGTH("/index.html"), &IndexCallback}
};

/*****************************************************************************/
/* Resource callback implementation                                          */
/* - use helper functions to prepare response, buffering is limited because  */
/*   of microcontroller environment usually suffers lack of memory           */
/*****************************************************************************/
static tHttpStatusCode FaviconCallback(
    void *const a)
{
  return HTTP_STATUS_OK;
}

static tHttpStatusCode IndexCallback(
    void *const a)
{
  const char *helloWorld = "Hello world";
  const void *const *parameters = { (const void *) &helloWorld };

  /* FIXME connection struct */
  Http_HelperSendStatusLine(&connection, HTTP_STATUS_OK);
  Http_HelperSendHeaderLine(&connection, "Content-Type", "text/html");
  Http_HelperSendHeaderLine(&connection, "Connection", "close");
  Http_HelperSendCRLF(&connection);
  Http_HelperSendMessageBodyParametered(&connection,
      "<html>" "<head>" "<meta http-equiv=\"Refresh\" content=\"1\" />"
      "</head>" "<body>" "<h1>Welcome to uCHttpServer!</h1>"
      "%s from uCHttpServer!" "</body>" "</html>", parameters);
  Http_HelperFlush(&connection);

  return HTTP_STATUS_OK;
}

static tHttpStatusCode AaaCallback(
    void *const a)
{
  return HTTP_STATUS_OK;
}

static tHttpStatusCode AbaCallback(
    void *const a)
{
  return HTTP_STATUS_OK;
}

static tHttpStatusCode AbbCallback(
    void *const a)
{
  return HTTP_STATUS_OK;
}

static tHttpStatusCode AbcCallback(
    void *const a)
{
  return HTTP_STATUS_OK;
}

static tHttpStatusCode BbbCallback(
    void *const a)
{
  return HTTP_STATUS_OK;
}

static tHttpStatusCode CbbCallback(
    void *const a)
{
  return HTTP_STATUS_OK;
}

static tHttpStatusCode CccCallback(
    void *const a)
{
  return HTTP_STATUS_OK;
}

static tHttpStatusCode CeeeCallback(
    void *const a)
{
  return HTTP_STATUS_OK;
}
