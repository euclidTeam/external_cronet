/*
 * Summary: interface for all global variables of the library
 * Description: Deprecated, don't use
 *
 * Copy: See Copyright for the status of this software.
 */

#ifndef __XML_GLOBALS_H
#define __XML_GLOBALS_H

#include <libxml/xmlversion.h>

/*
 * This file was required to access global variables until version v2.12.0.
 *
 * These includes are for backward compatibility.
 */
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xmlerror.h>
#include <libxml/xmlIO.h>
#include <libxml/xmlsave.h>
#include <libxml/threads.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _xmlGlobalState xmlGlobalState;
typedef xmlGlobalState *xmlGlobalStatePtr;

XML_DEPRECATED XMLPUBFUN void
xmlInitGlobals(void);
XML_DEPRECATED XMLPUBFUN void
xmlCleanupGlobals(void);
XML_DEPRECATED XMLPUBFUN void
xmlInitializeGlobalState(xmlGlobalStatePtr gs);
XML_DEPRECATED XMLPUBFUN
xmlGlobalStatePtr xmlGetGlobalState(void);

#ifdef __cplusplus
}
#endif

#endif /* __XML_GLOBALS_H */
