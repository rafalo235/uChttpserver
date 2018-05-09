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
/* Type definitions                                                          */
/*****************************************************************************/

typedef enum SearchEngineResultTag
{
  SEARCH_ENGINE_FOUND,
  SEARCH_ENGINE_ONGOING,
  SEARCH_ENGINE_NOT_FOUND,
  SEARCH_ENGINE_BUFFER_EXCEEDED
} tSearchEngineResult;

typedef enum CompareEngineResultTag
{
  COMPARE_ENGINE_MATCH,
  COMPARE_ENGINE_ONGOING,
  COMPARE_ENGINE_NOT_MATCH
} tCompareEngineResult;

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
static unsigned int InitializeResourceSearchState(
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

static void SearchEngine_Init(
    tSearchEntity * const se, const void * array, unsigned int length,
    tGetElementByIdxCallback getElementByIdx, char * buffer);
static void SearchEngine_Reset(tSearchEntity * const conn);
static int SearchEngine_Finished(tSearchEntity * const conn);
static int SearchEngine_Compare(char a, char b);
static int SearchEngine_CopyInput(tSearchEntity * const conn, char input);
static tSearchEngineResult SearchEngine_Search(
    tSearchEntity * const conn, char input, unsigned int * idx);

static void CompareEngine_Init(void * const conn);
static tCompareEngineResult CompareEngine_Compare(
    void * const conn, char input, const tStringWithLength * pattern);

static const tStringWithLength * Utils_GetResourceByIdx(
   const void * arr, unsigned int idx);

static unsigned int Utils_SearchPattern(
    const char *pattern, const char *stream,
    unsigned int pattenlen, unsigned int streamlen);

static char Utils_ToLowerCase(char input);

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

const tStringWithLength SP = STRING_WITH_LENGTH(" ");
const tStringWithLength QUESTION_MARK = STRING_WITH_LENGTH(" ");

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
	{ "400", "Bad Request"},
	{ "403", "Forbidden"},
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

void Http_HelperSendStatusLine(
    tuCHttpServerState * const sm, tHttpStatusCode code)
{
  Http_SendNullTerminatedPortWrapper(sm, "HTTP/1.1 ");
  Http_SendNullTerminatedPortWrapper(sm, statuscodes[code][0]);
  Http_SendPortWrapper(sm, SP.str, SP.length);
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
	  sm->state = &InitializeResourceSearchState;
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

static unsigned int InitializeResourceSearchState(
    void * const conn, const char * data, unsigned int length)
{
  tuCHttpServerState * const sm = conn;
  SearchEngine_Init(&(sm->shared.searchEntity),
		    sm->resources,
		    sm->resourcesLength,
		    &Utils_GetResourceByIdx,
		    sm->parametersBuffer);
  sm->state = &ParseAbsPathResourceState;
  return 0;
}

static unsigned int ParseAbsPathResourceState(
    void * const conn, const char * data, unsigned int length)
{
  tuCHttpServerState * const sm = conn;
  unsigned int parsed;
  tSearchEngineResult result;

  if (SEARCH_ENGINE_ONGOING ==
      (result = SearchEngine_Search(
	 &(sm->shared.searchEntity), *data, &(sm->resourceIdx))))
    {
      parsed = 1;
    }
  else if (SEARCH_ENGINE_FOUND == result)
    {
      /* workaround */
      //parsed = 1;
      sm->byte = 0;
      /* fixme parse get parameters */
      sm->compareIdx = 0;
      sm->left = 0;
      sm->right = 0;
      parsed = 2;
      sm->state = &ParseHttpVersion;
    }
  else if (SEARCH_ENGINE_NOT_FOUND == result)
    {
      parsed = 1;
      /* todo error callback */
    }
  else if (SEARCH_ENGINE_BUFFER_EXCEEDED == result)
    {
      parsed = 1;
      /* todo error callback */
    }

  return parsed;
}

static unsigned int ParseResourceEnding(
    void * const conn, const char * data, unsigned int length)
{
  tuCHttpServerState * const sm = conn;
  unsigned int parsed = 1U;
  tCompareEngineResult spaceResult =
      CompareEngine_Compare(conn, *data, &SP);
  tCompareEngineResult questionMarkResult =
      CompareEngine_Compare(conn, *data, &QUESTION_MARK);

  if (COMPARE_ENGINE_MATCH == spaceResult)
    {
      sm->state = &ParseHttpVersion;
    }
  else if (COMPARE_ENGINE_MATCH == questionMarkResult)
    {
      sm->state = &ParseUrlEncodedFormName;
    }
  else if ((COMPARE_ENGINE_ONGOING != spaceResult) &&
      (COMPARE_ENGINE_ONGOING != questionMarkResult))
    {
      /* todo bad request */
      parsed = 0U;
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

/*****************************************************************************/
/* Search engine                                                             */
/*****************************************************************************/

static void SearchEngine_Init(
    tSearchEntity * const se, const void * array, unsigned int length,
    tGetElementByIdxCallback getElementByIdx, char * buffer)
{
  se->array = array;
  se->buffer = buffer;
  se->compareIdx = 0U;
  se->left = 0U;
  se->right = length - 1;
  se->length = length;
  se->getElementByIdx = getElementByIdx;
}

static void SearchEngine_Reset(tSearchEntity * const se)
{
  se->compareIdx = 0U;
}

static int SearchEngine_Finished(tSearchEntity * const se)
{
  int result = 0;
  if (se->left == se->right)
    {
      result = 1;
    }
  return result;
}

static int SearchEngine_Compare(char a, char b)
{
  int result;
  if (a > b)
    {
      result = 1;
    }
  else if (a < b)
    {
      result = -1;
    }
  else
    {
      result = 0;
    }
  return result;
}

static int SearchEngine_CopyInput(tSearchEntity * const se, char input)
{
  unsigned int result = 0;

  if (se->bufferIdx < se->bufferLength)
    {
      se->buffer[se->bufferIdx] = input;
      ++(se->bufferIdx);
      result = 1;
    }

  return result;
}

static tSearchEngineResult SearchEngine_Search(
    tSearchEntity * const se, char input, unsigned int * idx)
{
  tSearchEngineResult result = SEARCH_ENGINE_ONGOING;

  if (1 != SearchEngine_CopyInput(se, input))
    {
      result = SEARCH_ENGINE_BUFFER_EXCEEDED;
    }
  else
    {
      do
	{
	  const tStringWithLength * resWithLength;

	  *idx = ((se->right - se->left) >> 1) + se->left;
	  resWithLength = (se->getElementByIdx)(se->array, *idx);

	  if (se->compareIdx >= resWithLength->length)
	    {
	      if (1 == SearchEngine_Finished(se))
		{
		  result = SEARCH_ENGINE_NOT_FOUND;
		}
	      else
		{
		  SearchEngine_Reset(se);
		}
	    }
	  else
	    {
	      const char * input = se->buffer + se->compareIdx;
	      const char * resource = resWithLength->str + se->compareIdx;
	      int comparation = SearchEngine_Compare(*input, *resource);

	      if (0 == comparation)
		{
		  ++(se->compareIdx);

		  if (resWithLength->length == se->compareIdx)
		    {
		      result = SEARCH_ENGINE_FOUND;
		    }
		}
	      else
		{
		  if (1 == SearchEngine_Finished(se))
		    {
		      result = SEARCH_ENGINE_NOT_FOUND;
		    }
		  else if (0 < comparation)
		    {
		      se->left = *idx + 1;
		    }
		  else if (0 > comparation)
		    {
		      se->right = *idx - 1;
		    }
		}
	    }
	}
      while (
	  (se->compareIdx < se->bufferIdx) && /* Process all available chars */
	  SEARCH_ENGINE_ONGOING == result);   /* Process not finished */
    }

  return result;
}

static void CompareEngine_Init(void * const conn)
{
  tuCHttpServerState * const sm = conn;
  sm->compareIdx = 0U;
}

static tCompareEngineResult CompareEngine_Compare(
    void * const conn, char input, const tStringWithLength * pattern)
{
  tuCHttpServerState * const sm = conn;
  tCompareEngineResult result;

  if (pattern->str[sm->compareIdx] == input)
    {
      ++(sm->compareIdx);
      result = COMPARE_ENGINE_ONGOING;

      /* Check if all characters are processed */
      if (pattern->length == sm->compareIdx)
        {
          result = COMPARE_ENGINE_MATCH;
        }
    }
  else
    {
      result = COMPARE_ENGINE_NOT_MATCH;
    }

  return result;
}

static const tStringWithLength * Utils_GetResourceByIdx(
    const void * arr, unsigned int idx)
{
  const tResourceEntry (*resources)[] = arr;
  return &(((*resources)[idx]).name);
}

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

static char Utils_ToLowerCase(char input)
{
  char result = input;
  if ('A' <= input && 'Z' >= input)
    {
      result += 0x20;
    }
  return result;
}

static unsigned int Utils_SearchNullTerminatedPattern(
    const char * pattern, const char * input)
{
  unsigned int ret = 0;
  while (*pattern)
    {
      if (Utils_ToLowerCase(*pattern) == Utils_ToLowerCase(*input))
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
