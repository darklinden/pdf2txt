//========================================================================
//
// pdftotext.cc
//
// Copyright 1997-2003 Glyph & Cog, LLC
//
//========================================================================

#include "../aconf.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "../goo/parseargs.h"
#include "../goo/GString.h"
#include "../goo/gmem.h"
#include "GlobalParams.h"
#include "Object.h"
#include "Stream.h"
#include "Array.h"
#include "Dict.h"
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"
#include "PDFDoc.h"
#include "TextOutputDev.h"
#include "CharTypes.h"
#include "UnicodeMap.h"
#include "Error.h"
#include "config.h"

static void printInfoString(FILE *f, Dict *infoDict, const char *key,
	const char *text1, const char *text2,
	UnicodeMap *uMap);
static void printInfoDate(FILE *f, Dict *infoDict, const char *key,
	const char *fmt);

static int firstPage = 1;
static int lastPage = 0;
static GBool physLayout = gFalse;
static double fixedPitch = 0;
static GBool rawOrder = gFalse;
static GBool htmlMeta = gFalse;
static char textEncName[128] = "";
static char textEOL[16] = "";
static GBool noPageBreaks = gFalse;
static char ownerPassword[33] = "\001";
static char userPassword[33] = "\001";
static GBool quiet = gFalse;
static char cfgFileName[256] = "";
static GBool printVersion = gFalse;
static GBool printHelp = gFalse;

static ArgDesc argDesc[] = {
	{"-f",       argInt,      &firstPage,     0,
	"first page to convert"},
	{"-l",       argInt,      &lastPage,      0,
	"last page to convert"},
	{"-layout",  argFlag,     &physLayout,    0,
	"maintain original physical layout"},
	{"-fixed",   argFP,       &fixedPitch,    0,
	"assume fixed-pitch (or tabular) text"},
	{"-raw",     argFlag,     &rawOrder,      0,
	"keep strings in content stream order"},
	{"-htmlmeta", argFlag,   &htmlMeta,       0,
	"generate a simple HTML file, including the meta information"},
	{"-enc",     argString,   textEncName,    sizeof(textEncName),
	"output text encoding name"},
	{"-eol",     argString,   textEOL,        sizeof(textEOL),
	"output end-of-line convention (unix, dos, or mac)"},
	{"-nopgbrk", argFlag,     &noPageBreaks,  0,
	"don't insert page breaks between pages"},
	{"-opw",     argString,   ownerPassword,  sizeof(ownerPassword),
	"owner password (for encrypted files)"},
	{"-upw",     argString,   userPassword,   sizeof(userPassword),
	"user password (for encrypted files)"},
	{"-q",       argFlag,     &quiet,         0,
	"don't print any messages or errors"},
	{"-cfg",     argString,   cfgFileName,    sizeof(cfgFileName),
	"configuration file to use in place of .xpdfrc"},
	{"-v",       argFlag,     &printVersion,  0,
	"print copyright and version info"},
	{"-h",       argFlag,     &printHelp,     0,
	"print usage information"},
	{"-help",    argFlag,     &printHelp,     0,
	"print usage information"},
	{"--help",   argFlag,     &printHelp,     0,
	"print usage information"},
	{"-?",       argFlag,     &printHelp,     0,
	"print usage information"},
	{NULL}
};

Unicode* GetUnicodeString(const wchar_t*str, int length)
{
	Unicode * ucstring = new Unicode[length + 1];
	int j;

	if (ucstring == NULL)
		return NULL;

	/* it is almost safe to transform from UCS2 to UCS4 this way */
	for( j = 0; j < length; j++)
		ucstring[j] = str[j];

	ucstring[j] = 0;
	return ucstring;
}

int main(int argc, char *argv[]) {
	PDFDoc *doc;
	GString *fileName;
	GString *textFileName;
	GString *ownerPW, *userPW;
	TextOutputDev *textOut;
	FILE *f;
	UnicodeMap *uMap;
	Object info;
	GBool ok;
	char *p;
	int exitCode;

	exitCode = 99;

	/*
	// parse args
	ok = parseArgs(argDesc, &argc, argv);
	if (!ok || argc < 2 || argc > 3 || printVersion || printHelp) {
	fprintf(stderr, "pdftotext version %s\n", xpdfVersion);
	fprintf(stderr, "%s\n", xpdfCopyright);
	if (!printVersion) {
	printUsage("pdftotext", "<PDF-file> [<text-file>]", argDesc);
	}
	goto err0;
	}
	*/

	fileName = new GString("c:\\pdf\\test.pdf");
	//fileName = new GString(argv[1]);
	if (fixedPitch) {
		physLayout = gTrue;
	}

	// read config file
	globalParams = new GlobalParams(cfgFileName);
	if (textEncName[0]) {
		globalParams->setTextEncoding(textEncName);
	}
	//if (textEOL[0]) {
	//	if (!globalParams->setTextEOL(textEOL)) {
	//		fprintf(stderr, "Bad '-eol' value on command line\n");
	//	}
	//}
	//if (noPageBreaks) {
	//	globalParams->setTextPageBreaks(gFalse);
	//}
	//if (quiet) {
	//	globalParams->setErrQuiet(quiet);
	//}

	globalParams->setPSPaperHeight(0);
	globalParams->setPSPaperWidth(0);
	globalParams->setPSPaperSize("");
	globalParams->setPSEmbedCIDPostScript(true);
	globalParams->setPSEmbedCIDTrueType(true);
	globalParams->setPSEmbedTrueType(true);
	globalParams->setPSEmbedType1(true);
	globalParams->setTextPageBreaks(true);
	globalParams->setTextEOL("yes");
	globalParams->setEnableFreeType("yes");
	globalParams->setEnableT1lib("no");

	// get mapping to output encoding
	if (!(uMap = globalParams->getTextEncoding())) {
		error(errConfig, -1, "Couldn't get text encoding");
		delete fileName;
		goto err1;
	}

	// open PDF file
	if (ownerPassword[0] != '\001') {
		ownerPW = new GString(ownerPassword);
	} else {
		ownerPW = NULL;
	}
	if (userPassword[0] != '\001') {
		userPW = new GString(userPassword);
	} else {
		userPW = NULL;
	}
	doc = new PDFDoc(fileName, ownerPW, userPW);
	if (userPW) {
		delete userPW;
	}
	if (ownerPW) {
		delete ownerPW;
	}
	if (!doc->isOk()) {
		exitCode = 1;
		goto err2;
	}

	// check for copy permission
	if (!doc->okToCopy()) {
		error(errNotAllowed, -1,
			"Copying of text from this document is not allowed.");
		exitCode = 3;
		goto err2;
	}

	// construct text file name
	if (argc == 3) {
		textFileName = new GString(argv[2]);
	} else {
		p = fileName->getCString() + fileName->getLength() - 4;
		if (!strcmp(p, ".pdf") || !strcmp(p, ".PDF")) {
			textFileName = new GString(fileName->getCString(),
				fileName->getLength() - 4);
		} else {
			textFileName = fileName->copy();
		}
		textFileName->append(htmlMeta ? ".html" : ".txt");
	}

	// get page range
	if (firstPage < 1) {
		firstPage = 1;
	}
	if (lastPage < 1 || lastPage > doc->getNumPages()) {
		lastPage = doc->getNumPages();
	}

	// write text file
	textOut = new TextOutputDev(textFileName->getCString(),
		physLayout, fixedPitch, rawOrder, htmlMeta);
	if (textOut->isOk()) {
		////doc->displayPages(textOut, firstPage, lastPage, 72, 72, 0, gFalse, gTrue, gFalse);
		//doc->displayPages(textOut, 1, 1, 72, 72, 0, gFalse, gTrue, gFalse);
		//double xmin = 0;
		//double ymin = 0;
		//double xmax = 0;
		//double ymax = 0;
		Unicode *ucstring = new Unicode('a');
		double __x0, __y0;
		__x0 = 0;
		__y0 = 0;
		////////////////////////////////////////////////////////////////////////////////////////////////

		double x0, y0, x1, y1;
		long iFirstPageFound=0;
		long searchPage=0;
		GBool rc, startAtTop, startAtLast, backward;

		TextOutputDev FindPage(NULL, gTrue, 0.0, gFalse, gFalse);
		int length = 1;
		
		//Tratar de reservar la cadena
		//ucstring = GetUnicodeString(sText, length);
		//if (ucstring == NULL) {
		//	AfxMessageBox("Out of memory");
		//	return FALSE;
		//}

		startAtTop = gTrue;
		startAtLast = gFalse;
		backward = gFalse; 
		GBool m_SearchStarted = FALSE;
		//m_Selection.RemoveAll();
		int FIND_DPI = 150;
		//Si se desea buscar desde el principio

			searchPage=1;


		/* use fixed DPI and rotation, this would lead to somehwere
		* wrong positioning displaying more than 300 DPI document
		* don't think it's a problem thought
		*/
		doc->displayPage(&FindPage, searchPage,
			FIND_DPI, FIND_DPI, 0, gFalse, gTrue, gFalse);

		if (!FindPage.isOk()) {
			//AfxMessageBox("Error al mostrar la pagina");
			delete [] ucstring;
			return -1;
		}

		//Mientras haya que hacer
		while(true) {
			//Buscar el texto
			rc = FindPage.findText(ucstring, length,
				startAtTop, gTrue, startAtLast, gFalse,
				gFalse, backward, gFalse,&x0, &y0, &x1, &y1);
			//Si existen resultados, agregamos esta coincidencia a la lista
			if (rc) {
				printf("%f %f %f %f %d",x0, y0, x1, y1, searchPage);
				__x0=x0;
				__y0=y0;
				iFirstPageFound =searchPage;
				break;
			}

			// Ir a la siguiente pagina
			startAtTop = gTrue;
			startAtLast = gFalse;

			if (backward)	//Buscar hacia atras
				searchPage--;
			else
				searchPage++;

			//Si ya no hay paginas para buscar
			if (searchPage < 1 || searchPage > doc->getNumPages()) {
				/*delete [] ucstring;
				m_SearchPage=iFirstPageFound;
				return m_Selection.GetCount();
				*/
				break;
			}
			//Dibujar la siguiente pagina
			doc->displayPage(&FindPage, searchPage,
				FIND_DPI, FIND_DPI, 0, gFalse, gTrue, gFalse);

			if (!FindPage.isOk()) {
				//AfxMessageBox("Error al mostrar la pagina");
				delete [] ucstring;
				return -1;	//No hay resultados
			}
		}
		if(iFirstPageFound>0)
			m_SearchStarted=true;

		//Establecer la pagina de busqueda en la primer pagina
		//m_SearchPage=iFirstPageFound;
		delete [] ucstring;
		//return m_Selection.GetCount();
		////////////////////////////////////////////////////////////////////////////////////////////////




		//textOut->findTexts,1,gTrue,gTrue,gFalse,gTrue,gTrue,gFalse,gFalse,&xmin,&ymin,&xmax,&ymax);
		//printf("%f %f %f %f",xmin,ymin,xmax,ymax);
	} else {
		delete textOut;
		exitCode = 2;
		goto err3;
	}
	delete textOut;

	// write end of HTML file
	if (htmlMeta) {
		if (!textFileName->cmp("-")) {
			f = stdout;
		} else {
			if (!(f = fopen(textFileName->getCString(), "ab"))) {
				error(errIO, -1, "Couldn't open text file '{0:t}'", textFileName);
				exitCode = 2;
				goto err3;
			}
		}
		fputs("</pre>\n", f);
		fputs("</body>\n", f);
		fputs("</html>\n", f);
		if (f != stdout) {
			fclose(f);
		}
	}

	exitCode = 0;

	// clean up
err3:
	delete textFileName;
err2:
	delete doc;
	uMap->decRefCnt();
err1:
	delete globalParams;
err0:

	// check for memory leaks
	Object::memCheck(stderr);
	gMemReport(stderr);

	return exitCode;
}

static void printInfoString(FILE *f, Dict *infoDict, const char *key,
	const char *text1, const char *text2,
	UnicodeMap *uMap) {
		Object obj;
		GString *s1;
		GBool isUnicode;
		Unicode u;
		char buf[8];
		int i, n;

		if (infoDict->lookup(key, &obj)->isString()) {
			fputs(text1, f);
			s1 = obj.getString();
			if ((s1->getChar(0) & 0xff) == 0xfe &&
				(s1->getChar(1) & 0xff) == 0xff) {
					isUnicode = gTrue;
					i = 2;
			} else {
				isUnicode = gFalse;
				i = 0;
			}
			while (i < obj.getString()->getLength()) {
				if (isUnicode) {
					u = ((s1->getChar(i) & 0xff) << 8) |
						(s1->getChar(i+1) & 0xff);
					i += 2;
				} else {
					u = s1->getChar(i) & 0xff;
					++i;
				}
				n = uMap->mapUnicode(u, buf, sizeof(buf));
				fwrite(buf, 1, n, f);
			}
			fputs(text2, f);
		}
		obj.free();
}

static void printInfoDate(FILE *f, Dict *infoDict, const char *key,
	const char *fmt) {
		Object obj;
		char *s;

		if (infoDict->lookup(key, &obj)->isString()) {
			s = obj.getString()->getCString();
			if (s[0] == 'D' && s[1] == ':') {
				s += 2;
			}
			fprintf(f, fmt, s);
		}
		obj.free();
}
