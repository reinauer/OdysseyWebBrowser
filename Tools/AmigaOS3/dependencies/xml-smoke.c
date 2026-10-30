/* Validate the rebuilt parser with the existing native XSLT library. */
#include <libxml/parser.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char* xml = "<?xml version='1.0' encoding='UTF-8'?><root><value>68</value><text>&#233;</text></root>";
    const char* xsl = "<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'><xsl:template match='/'><result><xsl:value-of select='root/value + 20'/></result></xsl:template></xsl:stylesheet>";
    xmlDocPtr input = xmlReadMemory(xml, strlen(xml), "input.xml", NULL, XML_PARSE_NONET);
    xmlDocPtr styleDocument = xmlReadMemory(xsl, strlen(xsl), "style.xsl", NULL, XML_PARSE_NONET);
    xsltStylesheetPtr style = styleDocument ? xsltParseStylesheetDoc(styleDocument) : NULL;
    xmlDocPtr output = style && input ? xsltApplyStylesheet(style, input, NULL) : NULL;
    xmlChar* result = output ? xmlNodeGetContent(xmlDocGetRootElement(output)) : NULL;
    unsigned failures = !result || strcmp((const char*)result, "88");
    if (input) {
        xmlNodePtr textNode = xmlDocGetRootElement(input)->last;
        xmlChar* text = xmlNodeGetContent(textNode);
        failures += !text || strcmp((const char*)text, "\xc3\xa9");
        xmlFree(text);
    } else
        ++failures;
    xmlFree(result);
    xmlFreeDoc(output);
    if (style)
        xsltFreeStylesheet(style);
    xmlFreeDoc(input);
    xsltCleanupGlobals();
    xmlCleanupParser();
    printf("libxml2 %s: UTF-8 entity parsing and XSLT arithmetic\n", LIBXML_DOTTED_VERSION);
    printf("XML dependency: %u failures\n", failures);
    return failures ? 1 : 0;
}
