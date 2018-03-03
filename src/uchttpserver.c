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

static unsigned int ParseMethodState(
    void * const sm, const char * data, unsigned int length);
static unsigned int PostMethodState(
    void * const sm, const char * data, unsigned int length);
static unsigned int DetectUriState(
    void * const sm, const char * data, unsigned int length);
static unsigned int ParseAbsPathResourceState(
    void * const sm, const char * data, unsigned int length);
static unsigned int CallResourceState(
    void * const sm, const char * data, unsigned int length);

/*****************************************************************************/
/* Local functions (declarations)                                            */
/*****************************************************************************/

static unsigned int Utils_SearchPattern(
    const char *pattern, const char *stream,
    unsigned int pattenlen, unsigned int streamlen);
static int Utils_Compare(const char * a, const char * b);

/*****************************************************************************/
/* Local variables and constants                                             */
/*****************************************************************************/

tStringWithLength methods[] =
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

/*****************************************************************************/
/* Global connection functions                                               */
/*****************************************************************************/

void
Http_InitializeConnection(tuCHttpServerState * const sm,
			  const tResourceEntry (*resources)[],
			  unsigned int reslen)
{
  sm->state = &ParseMethodState;
  sm->resources = resources;
  sm->resourcesLength = reslen;
}

void Http_Input(tuCHttpServerState * const sm,
		const char * data, unsigned int length)
{
  unsigned int parsed;
  while (length)
    {
      parsed = sm->state(sm, data, length);
      length -= parsed;
      data += parsed;
    }
}

/*****************************************************************************/
/* Global helper functions                                                   */
/*****************************************************************************/

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
	  sm->currentMethod = foundidx;
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
	      sm->currentMethod = i; /* tHttpMethod */
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

	}
      else
	{
	  /* Host ?proxy? */

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

      if ('\0' == *toCheck &&
	  (' ' == *data || '?' == *data))
	{
	  /* Match! */
	  sm->state = &CallResourceState;
	  length = 0;
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
