/*
 uchttpserver.c

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

/*****************************************************************************/
/* Includes                                                                  */
/*****************************************************************************/

#include "uchttpserver.h"

/*****************************************************************************/
/* Defines                                                                   */
/*****************************************************************************/


/*****************************************************************************/
/* Connection states (declarations)                                          */
/*****************************************************************************/

/* Request line                                                              */
static unsigned int ParseMethodState(
    void * const sm, const char * data, unsigned int length);
static unsigned int PostMethodState(
    void * const sm, const char * data, unsigned int length);
static unsigned int DetectUriState(
    void * const sm, const char * data, unsigned int length);
static unsigned int ParseAbsPathResourceState(
    void * const sm, const char * data, unsigned int length);
static unsigned int ParseUrlEncodedFormName(
    void * const sm, const char * data, unsigned int length);
static unsigned int ParseUrlEncodedFormValue(
    void * const sm, const char * data, unsigned int length);
static unsigned int ParseHttpVersion(
    void * const sm, const char * data, unsigned int length);

/* Parse request header parameters                                           */
/* - used <left> integer paramter array index                                */
/* - used <right> integer parameter - parameter buffer index                 */
static unsigned int CheckHeaderEndState(
    void * const sm, const char * data, unsigned int length);
static unsigned int ParseParameterNameState(
    void * const sm, const char * data, unsigned int length);
static unsigned int ParseParameterValueState(
    void * const sm, const char * data, unsigned int length);
static unsigned int AnalyzeEntityState(
    void * const conn, const char * data, unsigned int length);
static unsigned int ParseUrlEncodedEntityName(
    void * const conn, const char * data, unsigned int length);
static unsigned int ParseUrlEncodedEntityValue(
    void * const conn, const char * data, unsigned int length);
static unsigned int CallResourceState(
    void * const sm, const char * data, unsigned int length);

/*****************************************************************************/
/* Local functions (declarations)                                            */
/*****************************************************************************/

static unsigned int Utils_SearchPattern(
    const char *pattern, const char *stream,
    unsigned int pattenlen, unsigned int streamlen);

static unsigned int Utils_SearchNullTerminatedPattern(
    const char * pattern, const char * input);

static int Utils_AtoiNullTerminated(const char * str);

static void Utils_AddParameterName(void * const conn);

static void Utils_AddParameterValue(void * const conn);

static void Utils_AddParameterCharacter(void * const conn, char ch);

static int Utils_Compare(const char * a, const char * b);

static void Utils_PrintParameter(
    void * const conn, const char * format, const void * param);

static void Utils_PrintString(
    void * const conn, const char * value);

static void Http_SendPortWrapper(
    void * const conn, const char * data, unsigned int length);

static void Http_SendNullTerminatedPortWrapper(
    void * const conn, const char * data);

/*****************************************************************************/
/* Local variables and constants                                             */
/*****************************************************************************/

/* ?todo with length */
const char SP[] = " ";
const char CRLF[] = "\r\n";
const char const ESCAPE_CHARACTER = '%';

const tStringWithLength methods[] =
    {
	 STRING_WITH_LENGTH("GET"), /* Most commonly used */
	 STRING_WITH_LENGTH("POST"),
	 STRING_WITH_LENGTH("OPTIONS"),
	 STRING_WITH_LENGTH("HEAD"),
	 STRING_WITH_LENGTH("PUT"),
	 STRING_WITH_LENGTH("DELETE"),
	 STRING_WITH_LENGTH("TRACE"),
	 STRING_WITH_LENGTH("CONNECT")
    };
const char * statuscodes[][2] =
    {
	{ "200", "OK" },
	{ "100", "Continue" },
	{ "404", "Not Found" },
	{ "500", "Server fault" }
    };

/*****************************************************************************/
/* Global connection functions                                               */
/*****************************************************************************/

void
Http_InitializeConnection(tuCHttpServerState * const sm,
			  tSendCallback send,
			  const tResourceEntry (*resources)[],
			  unsigned int reslen,
			  void * context)
{
  sm->state = &ParseMethodState;
  sm->send = send;
  sm->resources = resources;
  sm->resourcesLength = reslen;
  sm->context = context;
}

void Http_Input(tuCHttpServerState * const sm,
		const char * data, unsigned int length)
{
  unsigned int parsed;
  while (length ||
      (&CallResourceState == sm->state) ||
      (&CheckHeaderEndState == sm->state) ||
      (&AnalyzeEntityState == sm->state) ||
      (&ParseUrlEncodedEntityName == sm->state) ||
      (&ParseUrlEncodedEntityValue == sm->state))
    {
      parsed = sm->state(sm, data, length);
      length -= parsed;
      data += parsed;
    }
}

/*****************************************************************************/
/* Global helper functions                                                   */
/*****************************************************************************/

tHttpMethod Http_HelperGetMethod(tuCHttpServerState * const sm)
{
  return (tHttpMethod)sm->method;
}

void * Http_HelperGetContext(tuCHttpServerState * const sm)
{
  return sm->context;
}

void Http_HelperSendStatusLine(
    tuCHttpServerState * const sm, tHttpStatusCode code)
{
  Http_SendNullTerminatedPortWrapper(sm, "HTTP/1.1 ");
  Http_SendNullTerminatedPortWrapper(sm, statuscodes[code][0]);
  Http_SendNullTerminatedPortWrapper(sm, SP);
  Http_SendNullTerminatedPortWrapper(sm, statuscodes[code][1]);
  Http_SendNullTerminatedPortWrapper(sm, CRLF);
}

void Http_HelperSendHeaderLine(
    tuCHttpServerState * const sm, const char * name, const char * value)
{
  Http_SendNullTerminatedPortWrapper(sm, name);
  Http_SendNullTerminatedPortWrapper(sm, ": ");
  Http_SendNullTerminatedPortWrapper(sm, value);
  Http_SendNullTerminatedPortWrapper(sm, CRLF);
}

void Http_HelperSendCRLF(tuCHttpServerState * const sm)
{
  Http_SendNullTerminatedPortWrapper(sm, CRLF);
}

void Http_HelperSendMessageBody(
    tuCHttpServerState * const sm, const char * body)
{
  Http_SendNullTerminatedPortWrapper(sm, body);
}

void Http_HelperSendMessageBodyParametered(
    tuCHttpServerState * const sm,
    const char * body, const void * const * param)
{
  while ('\0' != (*body))
    {
      if (ESCAPE_CHARACTER == (*body))
	{
	  Utils_PrintParameter(sm, body, *param);
	  body += 2;
	  ++param;
	}
      else
	{
	  Http_SendPortWrapper(sm, body, 1);
	  ++body;
	}
    }
}

void Http_HelperSend(
    tuCHttpServerState * const sm, const char * data, unsigned int length)
{
  Http_SendPortWrapper(sm, data, length);
}

void Http_HelperSendParametered(
    tuCHttpServerState * const sm, const char * data, unsigned int length,
    const void * const * param)
{
  while (length--)
    {
      if (ESCAPE_CHARACTER == (*data))
	{
	  Utils_PrintParameter(sm, data, *param);
	  data += 2;
	  --length; /* extra character */
	  ++param;
	}
      else
	{
	  Http_SendPortWrapper(sm, data, 1);
	  ++data;
	}
    }

}

void Http_HelperFlush(tuCHttpServerState * const sm)
{
  if (0 != sm->bufferIdx)
    {
      sm->send(sm, sm->buffer, sm->bufferIdx);
      sm->bufferIdx = 0;
    }
}

const char * Http_HelperGetParameter(
    tuCHttpServerState * const sm, const char * param)
{
  unsigned int i;
  const char * result = NULL;

  for (i = 0; i < HTTP_PARAMETERS_MAX; i++)
    {
      if (NULL == sm->parameters[i][0])
	{
	  break;
	}
      else if (0 == Utils_SearchNullTerminatedPattern(param, sm->parameters[i][0]))
	{
	  result = sm->parameters[i][1];
	  break;
	}
    }

  return result;
}

/*****************************************************************************/
/* Connection states (definitions)                                           */
/*****************************************************************************/

/* Parse request states definitions */
static unsigned int ParseMethodState(
    void * const conn, const char * data, unsigned int length)
{
  static const char *found = NULL;
  static unsigned int tofoundlen = 0U;
  static unsigned char foundidx;
  tuCHttpServerState * const sm = conn;
  unsigned int i;
  unsigned int size = sizeof(methods)/sizeof(methods[0]);
  unsigned int parsed = 0;

  /* Pattern already started on stream,
   * check if it's continued */
  if (found != NULL)
    {
      unsigned int counted =
	  Utils_SearchPattern(found, data, tofoundlen, length);
      if (counted == tofoundlen)
	{
	  /* Match! */
	  parsed += tofoundlen;
	  sm->method = foundidx;
	  sm->state = &PostMethodState;
	  length = 0;
	}
      else if (counted == length)
	{
	  parsed += length;
	  /* Stream shorter than pattern,
	   * continue searching on next input */
	  found += length;
	  tofoundlen -= length;

	  length = 0;
	}
      else
	{
	  /* Pattern broken,
	   * continue searching other patterns */
	  found = NULL;
	  tofoundlen = 0U;
	  /* Leave <parsed> unchanged */
	}
    }

  while (length)
    {
      /* Linear search - not a lot of elements */
      for (i = 0; i < size; ++i)
	{
	  unsigned int counted =
	      Utils_SearchPattern(methods[i].str,
                  data, methods[i].length, length);
	  if (counted == methods[i].length)
	    {
	      /* Match! */
	      parsed += methods[i].length;
	      sm->method = i; /* tHttpMethod */
	      sm->state = &PostMethodState;
	      length = 0;
	      break;
	    }
	  else if (counted == length)
	    {
	      parsed += length;

	      /* To be continued */
	      /* FIXME stream ends with 'P' */
	      found = methods[i].str + length;
	      tofoundlen = methods[i].length - length;
	      foundidx = i;

	      /* Exit outer loop */
	      length = 0;
	      break;
	    }
	}
    }
  return parsed;
}


static unsigned int PostMethodState(
    void * const conn, const char * data, unsigned int length)
{
  tuCHttpServerState * const sm = conn;
  unsigned int parsed = 0;
  if (length)
    {
      if (' ' == *data)
	{
	  parsed = 1;
	  sm->state = &DetectUriState;
	}
      else
	{
	  /* Error - expected SP */
	  sm->state = &ParseMethodState;
	}
    }
  return parsed;
}

static unsigned int DetectUriState(
    void * const conn, const char * data, unsigned int length)
{
  tuCHttpServerState * const sm = conn;
  if (length)
    {
      if ('/' == *data)
	{
	  /* Most common - abs_path */
	  sm->state = &ParseAbsPathResourceState;
	  sm->compareIdx = 0;
	  sm->left = 0;
	  sm->right = sm->resourcesLength - 1;
	}
      else if ('*' == *data)
	{
	  /* Server request */
	  sm->state = &ParseMethodState;

	}
      else
	{
	  /* Host ?proxy? */
	  sm->state = &ParseMethodState;
	}
    }

  return 0;
}

static unsigned int ParseAbsPathResourceState(
    void * const conn, const char * data, unsigned int length)
{
  tuCHttpServerState * const sm = conn;
  unsigned int parsed = 0;

  while (length)
    {
      const tResourceEntry *res;
      const char * toCheck; //fixme exceed len

      sm->resourceIdx = ((sm->right - sm->left) >> 1) + sm->left;
      res = &((*sm->resources)[sm->resourceIdx]);
      toCheck = res->name.str + sm->compareIdx; //fixme exceed len

      if ('\0' == *toCheck)
	{
	  /* Match! */
	  sm->byte = 0;
	  /* fixme parse get parameters */
	  sm->compareIdx = 0;
	  sm->left = 0;
	  sm->right = 0;
	  length = 0;
	  ++parsed;

	  if (' ' == *data)
	    {
	      sm->state = &ParseHttpVersion;

	    }
	  else if ('?' == *data)
	    {
	      sm->state = &ParseUrlEncodedFormName;
	      Utils_AddParameterName(conn);
	    }
	}
      else
	{
	  int res = Utils_Compare(data, toCheck);
	  if (0 == res)
	    {
	      /* Match for now */
	      ++toCheck;
	      ++(sm->compareIdx);
	      --length;
	      ++data;
	      ++parsed;
	    }
	  else
	    {
	      if (sm->left == sm->right)
		{
		  /* Not found! */
		  sm->state = &ParseMethodState;
		}
	      else
		{
		  /* Another resource need to be checked */
		  if (0 < res)
		    {
		      sm->left = sm->resourceIdx + 1;
		    }
		  else /* if (0 > res) */
		    {
		      sm->right = sm->resourceIdx - 1;
		    }
		}
	    }
	}
    }

  return parsed;
}

static unsigned int ParseUrlEncodedFormName(
    void * const conn, const char * data, unsigned int length)
{
  tuCHttpServerState * const sm = conn;
  unsigned int parsed = 0;

  if ('=' == *data)
    {
      sm->state = &ParseUrlEncodedFormValue;
      Utils_AddParameterCharacter(conn, '\0');
      Utils_AddParameterValue(conn);
      parsed = 1;
    }
  else if (' ' == *data)
    {
      sm->state = &ParseHttpVersion;
      Utils_AddParameterCharacter(conn, '\0');
      parsed = 1;
    }
  else
    {
      Utils_AddParameterCharacter(conn, *data);
      parsed = 1;
    }

  return parsed;
}

static unsigned int ParseUrlEncodedFormValue(
    void * const conn, const char * data, unsigned int length)
{
  tuCHttpServerState * const sm = conn;
  unsigned int parsed = 0;

  if ('&' == *data)
    {
      sm->state = &ParseUrlEncodedFormName;
      Utils_AddParameterCharacter(conn, '\0');
      Utils_AddParameterName(conn);
      parsed = 1;
    }
  else if (' ' == *data)
    {
      sm->state = &ParseHttpVersion;
      Utils_AddParameterCharacter(conn, '\0');
      parsed = 1;
    }
  else
    {
      Utils_AddParameterCharacter(conn, *data);
      parsed = 1;
    }

  return parsed;
}

static unsigned int ParseHttpVersion(
    void * const conn, const char * data, unsigned int length)
{
  tuCHttpServerState * const sm = conn;
  unsigned int parsed = 0;

  if (2 == sm->compareIdx)
    {
      sm->compareIdx = 0;
      sm->state = &CheckHeaderEndState;
      parsed = 0;
    }
  else if (CRLF[sm->compareIdx] == *data)
    {
      ++(sm->compareIdx);
      parsed = 1;
    }
  else
    {
      parsed = 1;
    }

  return parsed;
}

static unsigned int CheckHeaderEndState(
    void * const conn, const char * data, unsigned int length)
{
  tuCHttpServerState * const sm = conn;
  unsigned int parsed = 0;
  if (2 == sm->compareIdx)
    {
      sm->state = &AnalyzeEntityState;
      parsed = 0;
    }
  else if (CRLF[sm->compareIdx] == *data)
    {
      ++(sm->compareIdx);
      parsed = 1;
    }
  else
    {
      sm->state = &ParseParameterNameState;
      Utils_AddParameterName(conn);

      parsed = 0;
    }

  return parsed;
}

static unsigned int ParseParameterNameState(
    void * const conn, const char * data, unsigned int length)
{
  tuCHttpServerState * const sm = conn;
  unsigned int parsed = 0;

  if (':' == *data)
    {
      /* Set next state */
      sm->state = &ParseParameterValueState;
      Utils_AddParameterCharacter(conn, '\0');
      Utils_AddParameterValue(conn);

      parsed = 1;
    }
  else
    {
      Utils_AddParameterCharacter(conn, *data);
      parsed = 1;
    }
  return parsed;
}

static unsigned int ParseParameterValueState(
    void * const conn, const char * data, unsigned int length)
{
  tuCHttpServerState * const sm = conn;
  unsigned int * bufferIdx = &(sm->right);
  unsigned int parsed = 0;

  if (2 == sm->compareIdx)
    {
      Utils_AddParameterCharacter(conn, '\0');
      sm->compareIdx = 0;
      sm->state = &CheckHeaderEndState;
    }
  else if (*data == CRLF[sm->compareIdx])
    {
      ++(sm->compareIdx);
      parsed = 1;
    }
  else if (' ' == *data || '\t' == *data)
    {
      /* Ignore Linear White Space */
      parsed = 1;
    }
  else
    {
      Utils_AddParameterCharacter(conn, *data);
      parsed = 1;
    }

  return parsed;
}

static unsigned int AnalyzeEntityState(
    void * const conn, const char * data, unsigned int length)
{
  tuCHttpServerState * const sm = conn;
  const char * content = Http_HelperGetParameter(sm, "Content-Type");
  const char * contentLength = Http_HelperGetParameter(sm, "Content-Length");

  if (NULL == content)
    {
      sm->state = &CallResourceState;
    }
  else if (0 == Utils_SearchNullTerminatedPattern(
      "application/x-www-form-urlencoded", content))
    {
      sm->state = &ParseUrlEncodedEntityName;
      if (NULL != contentLength)
	{
	  sm->contentLength = Utils_AtoiNullTerminated(contentLength);
	}
      else
	{
	  sm->contentLength = 0;
	}
      Utils_AddParameterName(conn);
    }
  else
    {
      sm->state = &CallResourceState;
    }
  return 0;
}

static unsigned int ParseUrlEncodedEntityName(
    void * const conn, const char * data, unsigned int length)
{
  tuCHttpServerState * const sm = conn;
  unsigned int parsed = 0;

  if (0U == sm->contentLength)
    {
      sm->state = &CallResourceState;
      Utils_AddParameterCharacter(conn, '\0');
      parsed = 0U;
    }
  else if ('=' == *data)
    {
      sm->state = &ParseUrlEncodedEntityValue;
      Utils_AddParameterCharacter(conn, '\0');
      Utils_AddParameterValue(conn);
      sm->contentLength--;
      parsed = 1;
    }
  else
    {
      Utils_AddParameterCharacter(conn, *data);
      sm->contentLength--;
      parsed = 1;
    }

  return parsed;
}

static unsigned int ParseUrlEncodedEntityValue(
    void * const conn, const char * data, unsigned int length)
{
  tuCHttpServerState * const sm = conn;
  unsigned int parsed = 0;

  if (0U == sm->contentLength)
    {
      sm->state = &CallResourceState;
      Utils_AddParameterCharacter(conn, '\0');
      parsed = 0U;
    }
  else if ('&' == *data)
    {
      sm->state = &ParseUrlEncodedEntityName;
      Utils_AddParameterCharacter(conn, '\0');
      Utils_AddParameterName(conn);
      sm->contentLength--;
      parsed = 1;
    }
  else
    {
      Utils_AddParameterCharacter(conn, *data);
      sm->contentLength--;
      parsed = 1;
    }

  return parsed;
}

static unsigned int CallResourceState(
    void * const conn, const char * data, unsigned int length)
{
  tuCHttpServerState * const sm = conn;

  (*sm->resources)[sm->resourceIdx].callback(conn);
  /* End of parsing request */
  sm->state = &ParseMethodState;
  return 0;
}

/*****************************************************************************/
/* Local functions (definitions)                                             */
/*****************************************************************************/

static unsigned int Utils_SearchPattern(
    const char *pattern, const char *stream,
    unsigned int patternlen, unsigned int streamlen)
{
  unsigned int ret = 0;
  while ((patternlen--) && (streamlen--))
    {
      if (*pattern == *stream)
	{
	  ++ret;
	}
      else
	{
	  break;
	}
    }
  return ret;
}

static unsigned int Utils_SearchNullTerminatedPattern(
    const char * pattern, const char * input)
{
  unsigned int ret = 0;
  while (*pattern)
    {
      if (*pattern == *input)
	{
	  ++pattern;
	  ++input;
	}
      else
	{
	  ret = 1;
	  break;
	}
    }
  /* Check null termination */
  if (*pattern != *input)
    {
      ret = 1;
    }
  return ret;
}

static int Utils_AtoiNullTerminated(const char * str)
{
  int multiplier = 1;
  int result = 0;

  if (*str == '-')
    {
      multiplier = -1;
      ++str;
    }
  while (*str)
    {
      if (('0' <= *str) && ('9' >= *str))
	{
	  result *= 10;
	  result += ((int)*str) - ((int)'0');
	}
      else
	{
	  /* Exit on any other character */
	  break;
	}
      ++str;
    }
  return result * multiplier;
}

static void Utils_AddParameterName(void * const conn)
{
  tuCHttpServerState * const sm = conn;
  unsigned int * paramIdx = &(sm->left);
  unsigned int * bufferIdx = &(sm->right);

  if (*paramIdx <= HTTP_PARAMETERS_MAX)
    {
      sm->parameters[*paramIdx][0] = &(sm->parametersBuffer[*bufferIdx]);
    }
}

static void Utils_AddParameterValue(void * const conn)
{
  tuCHttpServerState * const sm = conn;
  unsigned int * paramIdx = &(sm->left);
  unsigned int * bufferIdx = &(sm->right);

  if (*paramIdx < HTTP_PARAMETERS_MAX)
    {
      sm->parameters[*paramIdx][1] = &(sm->parametersBuffer[*bufferIdx]);
      ++(*paramIdx);
    }
}

static void Utils_AddParameterCharacter(void * const conn, char ch)
{
  tuCHttpServerState * const sm = conn;
  unsigned int * bufferIdx = &(sm->right);

  if (*bufferIdx < (HTTP_PARAMETERS_BUFFER_LENGTH - 1))
    {
      sm->parametersBuffer[*bufferIdx] = ch;
      ++(*bufferIdx);
    }
  else if (*bufferIdx == (HTTP_PARAMETERS_BUFFER_LENGTH - 1) &&
      sm->parametersBuffer[*bufferIdx])
    {
      sm->parametersBuffer[*bufferIdx] = '\0';
    }
}

static int Utils_Compare(const char * a, const char * b)
{
  int result;
  if (*a > *b)
    {
      result = 1;
    }
  else if (*a < *b)
    {
      result = -1;
    }
  else
    {
      result = 0;
    }
  return result;
}

static void Utils_PrintParameter(
    void * const conn, const char * format, const void * param)
{
  if ('s' == format[1])
    {
      Utils_PrintString(conn, param);
    }
  else if (ESCAPE_CHARACTER == format[1])
    {
      Http_SendPortWrapper(conn, &ESCAPE_CHARACTER, 1);
    }
}

static void Utils_PrintString(
    void * const conn, const char * value)
{
  Http_SendNullTerminatedPortWrapper(conn, value);
}

static void
Http_SendPortWrapper(
    void * const conn, const char * data, unsigned int length)
{
  tuCHttpServerState * const sm = conn;

  while (length)
    {
      if (sm->bufferIdx < HTTP_BUFFER_LENGTH)
	{
	  sm->buffer[sm->bufferIdx] = *data;
	  ++data;
	  --length;
	  ++(sm->bufferIdx);
	}
      else
	{
	  sm->send(conn, sm->buffer, HTTP_BUFFER_LENGTH);
	  sm->bufferIdx = 0;
	}
    }
}

static void
Http_SendNullTerminatedPortWrapper(
    void * const conn, const char * data)
{
  tuCHttpServerState * const sm = conn;
  while ('\0' != (*data))
    {
      Http_SendPortWrapper(conn, data, 1);
      ++data;
    }
}
