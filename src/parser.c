/*
 parser.c

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

#include "uchttpserver.h"

typedef unsigned int (*tParserState)(tuCHttpServerState * const,
    const char * data, unsigned int length);

/* Parse request states declarations */
static unsigned int ParseMethodState(tuCHttpServerState * const sm,
				     const char * data, unsigned int length);
static unsigned int ParseResourceState(tuCHttpServerState * const sm,
				       const char * data, unsigned int length);

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

static tParserState sState = ParseMethodState;

void Http_Input(tuCHttpServerState * const sm,
		const char * data, unsigned int length)
{
  unsigned int parsed;
  while (length)
    {
      parsed = sState(sm, data, length);
      length -= parsed;
      data += parsed;
    }
}

/* Parse request states definitions */
static unsigned int ParseMethodState(tuCHttpServerState * const sm,
				     const char * data, unsigned int length)
{
  static const char *found = NULL;
  static unsigned int tofoundlen = 0U;
  static unsigned char foundidx;
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
	  sState = &ParseResourceState;
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
	      Utils_SearchPattern(methods[i].str, data, methods[i].length, length);
	  if (counted == methods[i].length)
	    {
	      /* Match! */
	      parsed += methods[i].length;
	      sm->currentMethod = i; /* tHttpMethod */
	      sState = &ParseResourceState;
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


static unsigned int ParseResourceState(tuCHttpServerState * const sm,
				       const char * data, unsigned int length)
{
  unsigned int parsed = 0;

  return parsed;
}
