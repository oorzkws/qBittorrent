/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
 *
 * This program is free software); you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation); either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY); without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program); if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "htmlparser.h"

#include <QDebug>
#include <QSet>
#include <QStringList>
#include <QTextCodec>

// maps the HTML entity name to the Unicode codepoint
const QHash<QString, ushort> NAME2CODEPOINT {
    {"AElig",    0x00c6}, // latin capital letter AE = latin capital ligature AE, U+00C6 ISOlat1
    {"Aacute",   0x00c1}, // latin capital letter A with acute, U+00C1 ISOlat1
    {"Acirc",    0x00c2}, // latin capital letter A with circumflex, U+00C2 ISOlat1
    {"Agrave",   0x00c0}, // latin capital letter A with grave = latin capital letter A grave, U+00C0 ISOlat1
    {"Alpha",    0x0391}, // greek capital letter alpha, U+0391
    {"Aring",    0x00c5}, // latin capital letter A with ring above = latin capital letter A ring, U+00C5 ISOlat1
    {"Atilde",   0x00c3}, // latin capital letter A with tilde, U+00C3 ISOlat1
    {"Auml",     0x00c4}, // latin capital letter A with diaeresis, U+00C4 ISOlat1
    {"Beta",     0x0392}, // greek capital letter beta, U+0392
    {"Ccedil",   0x00c7}, // latin capital letter C with cedilla, U+00C7 ISOlat1
    {"Chi",      0x03a7}, // greek capital letter chi, U+03A7
    {"Dagger",   0x2021}, // double dagger, U+2021 ISOpub
    {"Delta",    0x0394}, // greek capital letter delta, U+0394 ISOgrk3
    {"ETH",      0x00d0}, // latin capital letter ETH, U+00D0 ISOlat1
    {"Eacute",   0x00c9}, // latin capital letter E with acute, U+00C9 ISOlat1
    {"Ecirc",    0x00ca}, // latin capital letter E with circumflex, U+00CA ISOlat1
    {"Egrave",   0x00c8}, // latin capital letter E with grave, U+00C8 ISOlat1
    {"Epsilon",  0x0395}, // greek capital letter epsilon, U+0395
    {"Eta",      0x0397}, // greek capital letter eta, U+0397
    {"Euml",     0x00cb}, // latin capital letter E with diaeresis, U+00CB ISOlat1
    {"Gamma",    0x0393}, // greek capital letter gamma, U+0393 ISOgrk3
    {"Iacute",   0x00cd}, // latin capital letter I with acute, U+00CD ISOlat1
    {"Icirc",    0x00ce}, // latin capital letter I with circumflex, U+00CE ISOlat1
    {"Igrave",   0x00cc}, // latin capital letter I with grave, U+00CC ISOlat1
    {"Iota",     0x0399}, // greek capital letter iota, U+0399
    {"Iuml",     0x00cf}, // latin capital letter I with diaeresis, U+00CF ISOlat1
    {"Kappa",    0x039a}, // greek capital letter kappa, U+039A
    {"Lambda",   0x039b}, // greek capital letter lambda, U+039B ISOgrk3
    {"Mu",       0x039c}, // greek capital letter mu, U+039C
    {"Ntilde",   0x00d1}, // latin capital letter N with tilde, U+00D1 ISOlat1
    {"Nu",       0x039d}, // greek capital letter nu, U+039D
    {"OElig",    0x0152}, // latin capital ligature OE, U+0152 ISOlat2
    {"Oacute",   0x00d3}, // latin capital letter O with acute, U+00D3 ISOlat1
    {"Ocirc",    0x00d4}, // latin capital letter O with circumflex, U+00D4 ISOlat1
    {"Ograve",   0x00d2}, // latin capital letter O with grave, U+00D2 ISOlat1
    {"Omega",    0x03a9}, // greek capital letter omega, U+03A9 ISOgrk3
    {"Omicron",  0x039f}, // greek capital letter omicron, U+039F
    {"Oslash",   0x00d8}, // latin capital letter O with stroke = latin capital letter O slash, U+00D8 ISOlat1
    {"Otilde",   0x00d5}, // latin capital letter O with tilde, U+00D5 ISOlat1
    {"Ouml",     0x00d6}, // latin capital letter O with diaeresis, U+00D6 ISOlat1
    {"Phi",      0x03a6}, // greek capital letter phi, U+03A6 ISOgrk3
    {"Pi",       0x03a0}, // greek capital letter pi, U+03A0 ISOgrk3
    {"Prime",    0x2033}, // double prime = seconds = inches, U+2033 ISOtech
    {"Psi",      0x03a8}, // greek capital letter psi, U+03A8 ISOgrk3
    {"Rho",      0x03a1}, // greek capital letter rho, U+03A1
    {"Scaron",   0x0160}, // latin capital letter S with caron, U+0160 ISOlat2
    {"Sigma",    0x03a3}, // greek capital letter sigma, U+03A3 ISOgrk3
    {"THORN",    0x00de}, // latin capital letter THORN, U+00DE ISOlat1
    {"Tau",      0x03a4}, // greek capital letter tau, U+03A4
    {"Theta",    0x0398}, // greek capital letter theta, U+0398 ISOgrk3
    {"Uacute",   0x00da}, // latin capital letter U with acute, U+00DA ISOlat1
    {"Ucirc",    0x00db}, // latin capital letter U with circumflex, U+00DB ISOlat1
    {"Ugrave",   0x00d9}, // latin capital letter U with grave, U+00D9 ISOlat1
    {"Upsilon",  0x03a5}, // greek capital letter upsilon, U+03A5 ISOgrk3
    {"Uuml",     0x00dc}, // latin capital letter U with diaeresis, U+00DC ISOlat1
    {"Xi",       0x039e}, // greek capital letter xi, U+039E ISOgrk3
    {"Yacute",   0x00dd}, // latin capital letter Y with acute, U+00DD ISOlat1
    {"Yuml",     0x0178}, // latin capital letter Y with diaeresis, U+0178 ISOlat2
    {"Zeta",     0x0396}, // greek capital letter zeta, U+0396
    {"aacute",   0x00e1}, // latin small letter a with acute, U+00E1 ISOlat1
    {"acirc",    0x00e2}, // latin small letter a with circumflex, U+00E2 ISOlat1
    {"acute",    0x00b4}, // acute accent = spacing acute, U+00B4 ISOdia
    {"aelig",    0x00e6}, // latin small letter ae = latin small ligature ae, U+00E6 ISOlat1
    {"agrave",   0x00e0}, // latin small letter a with grave = latin small letter a grave, U+00E0 ISOlat1
    {"alefsym",  0x2135}, // alef symbol = first transfinite cardinal, U+2135 NEW
    {"alpha",    0x03b1}, // greek small letter alpha, U+03B1 ISOgrk3
    {"amp",      0x0026}, // ampersand, U+0026 ISOnum
    {"and",      0x2227}, // logical and = wedge, U+2227 ISOtech
    {"ang",      0x2220}, // angle, U+2220 ISOamso
    {"aring",    0x00e5}, // latin small letter a with ring above = latin small letter a ring, U+00E5 ISOlat1
    {"asymp",    0x2248}, // almost equal to = asymptotic to, U+2248 ISOamsr
    {"atilde",   0x00e3}, // latin small letter a with tilde, U+00E3 ISOlat1
    {"auml",     0x00e4}, // latin small letter a with diaeresis, U+00E4 ISOlat1
    {"bdquo",    0x201e}, // double low-9 quotation mark, U+201E NEW
    {"beta",     0x03b2}, // greek small letter beta, U+03B2 ISOgrk3
    {"brvbar",   0x00a6}, // broken bar = broken vertical bar, U+00A6 ISOnum
    {"bull",     0x2022}, // bullet = black small circle, U+2022 ISOpub
    {"cap",      0x2229}, // intersection = cap, U+2229 ISOtech
    {"ccedil",   0x00e7}, // latin small letter c with cedilla, U+00E7 ISOlat1
    {"cedil",    0x00b8}, // cedilla = spacing cedilla, U+00B8 ISOdia
    {"cent",     0x00a2}, // cent sign, U+00A2 ISOnum
    {"chi",      0x03c7}, // greek small letter chi, U+03C7 ISOgrk3
    {"circ",     0x02c6}, // modifier letter circumflex accent, U+02C6 ISOpub
    {"clubs",    0x2663}, // black club suit = shamrock, U+2663 ISOpub
    {"cong",     0x2245}, // approximately equal to, U+2245 ISOtech
    {"copy",     0x00a9}, // copyright sign, U+00A9 ISOnum
    {"crarr",    0x21b5}, // downwards arrow with corner leftwards = carriage return, U+21B5 NEW
    {"cup",      0x222a}, // union = cup, U+222A ISOtech
    {"curren",   0x00a4}, // currency sign, U+00A4 ISOnum
    {"dArr",     0x21d3}, // downwards double arrow, U+21D3 ISOamsa
    {"dagger",   0x2020}, // dagger, U+2020 ISOpub
    {"darr",     0x2193}, // downwards arrow, U+2193 ISOnum
    {"deg",      0x00b0}, // degree sign, U+00B0 ISOnum
    {"delta",    0x03b4}, // greek small letter delta, U+03B4 ISOgrk3
    {"diams",    0x2666}, // black diamond suit, U+2666 ISOpub
    {"divide",   0x00f7}, // division sign, U+00F7 ISOnum
    {"eacute",   0x00e9}, // latin small letter e with acute, U+00E9 ISOlat1
    {"ecirc",    0x00ea}, // latin small letter e with circumflex, U+00EA ISOlat1
    {"egrave",   0x00e8}, // latin small letter e with grave, U+00E8 ISOlat1
    {"empty",    0x2205}, // empty set = null set = diameter, U+2205 ISOamso
    {"emsp",     0x2003}, // em space, U+2003 ISOpub
    {"ensp",     0x2002}, // en space, U+2002 ISOpub
    {"epsilon",  0x03b5}, // greek small letter epsilon, U+03B5 ISOgrk3
    {"equiv",    0x2261}, // identical to, U+2261 ISOtech
    {"eta",      0x03b7}, // greek small letter eta, U+03B7 ISOgrk3
    {"eth",      0x00f0}, // latin small letter eth, U+00F0 ISOlat1
    {"euml",     0x00eb}, // latin small letter e with diaeresis, U+00EB ISOlat1
    {"euro",     0x20ac}, // euro sign, U+20AC NEW
    {"exist",    0x2203}, // there exists, U+2203 ISOtech
    {"fnof",     0x0192}, // latin small f with hook = function = florin, U+0192 ISOtech
    {"forall",   0x2200}, // for all, U+2200 ISOtech
    {"frac12",   0x00bd}, // vulgar fraction one half = fraction one half, U+00BD ISOnum
    {"frac14",   0x00bc}, // vulgar fraction one quarter = fraction one quarter, U+00BC ISOnum
    {"frac34",   0x00be}, // vulgar fraction three quarters = fraction three quarters, U+00BE ISOnum
    {"frasl",    0x2044}, // fraction slash, U+2044 NEW
    {"gamma",    0x03b3}, // greek small letter gamma, U+03B3 ISOgrk3
    {"ge",       0x2265}, // greater-than or equal to, U+2265 ISOtech
    {"gt",       0x003e}, // greater-than sign, U+003E ISOnum
    {"hArr",     0x21d4}, // left right double arrow, U+21D4 ISOamsa
    {"harr",     0x2194}, // left right arrow, U+2194 ISOamsa
    {"hearts",   0x2665}, // black heart suit = valentine, U+2665 ISOpub
    {"hellip",   0x2026}, // horizontal ellipsis = three dot leader, U+2026 ISOpub
    {"iacute",   0x00ed}, // latin small letter i with acute, U+00ED ISOlat1
    {"icirc",    0x00ee}, // latin small letter i with circumflex, U+00EE ISOlat1
    {"iexcl",    0x00a1}, // inverted exclamation mark, U+00A1 ISOnum
    {"igrave",   0x00ec}, // latin small letter i with grave, U+00EC ISOlat1
    {"image",    0x2111}, // blackletter capital I = imaginary part, U+2111 ISOamso
    {"infin",    0x221e}, // infinity, U+221E ISOtech
    {"int",      0x222b}, // integral, U+222B ISOtech
    {"iota",     0x03b9}, // greek small letter iota, U+03B9 ISOgrk3
    {"iquest",   0x00bf}, // inverted question mark = turned question mark, U+00BF ISOnum
    {"isin",     0x2208}, // element of, U+2208 ISOtech
    {"iuml",     0x00ef}, // latin small letter i with diaeresis, U+00EF ISOlat1
    {"kappa",    0x03ba}, // greek small letter kappa, U+03BA ISOgrk3
    {"lArr",     0x21d0}, // leftwards double arrow, U+21D0 ISOtech
    {"lambda",   0x03bb}, // greek small letter lambda, U+03BB ISOgrk3
    {"lang",     0x2329}, // left-pointing angle bracket = bra, U+2329 ISOtech
    {"laquo",    0x00ab}, // left-pointing double angle quotation mark = left pointing guillemet, U+00AB ISOnum
    {"larr",     0x2190}, // leftwards arrow, U+2190 ISOnum
    {"lceil",    0x2308}, // left ceiling = apl upstile, U+2308 ISOamsc
    {"ldquo",    0x201c}, // left double quotation mark, U+201C ISOnum
    {"le",       0x2264}, // less-than or equal to, U+2264 ISOtech
    {"lfloor",   0x230a}, // left floor = apl downstile, U+230A ISOamsc
    {"lowast",   0x2217}, // asterisk operator, U+2217 ISOtech
    {"loz",      0x25ca}, // lozenge, U+25CA ISOpub
    {"lrm",      0x200e}, // left-to-right mark, U+200E NEW RFC 2070
    {"lsaquo",   0x2039}, // single left-pointing angle quotation mark, U+2039 ISO proposed
    {"lsquo",    0x2018}, // left single quotation mark, U+2018 ISOnum
    {"lt",       0x003c}, // less-than sign, U+003C ISOnum
    {"macr",     0x00af}, // macron = spacing macron = overline = APL overbar, U+00AF ISOdia
    {"mdash",    0x2014}, // em dash, U+2014 ISOpub
    {"micro",    0x00b5}, // micro sign, U+00B5 ISOnum
    {"middot",   0x00b7}, // middle dot = Georgian comma = Greek middle dot, U+00B7 ISOnum
    {"minus",    0x2212}, // minus sign, U+2212 ISOtech
    {"mu",       0x03bc}, // greek small letter mu, U+03BC ISOgrk3
    {"nabla",    0x2207}, // nabla = backward difference, U+2207 ISOtech
    {"nbsp",     0x00a0}, // no-break space = non-breaking space, U+00A0 ISOnum
    {"ndash",    0x2013}, // en dash, U+2013 ISOpub
    {"ne",       0x2260}, // not equal to, U+2260 ISOtech
    {"ni",       0x220b}, // contains as member, U+220B ISOtech
    {"not",      0x00ac}, // not sign, U+00AC ISOnum
    {"notin",    0x2209}, // not an element of, U+2209 ISOtech
    {"nsub",     0x2284}, // not a subset of, U+2284 ISOamsn
    {"ntilde",   0x00f1}, // latin small letter n with tilde, U+00F1 ISOlat1
    {"nu",       0x03bd}, // greek small letter nu, U+03BD ISOgrk3
    {"oacute",   0x00f3}, // latin small letter o with acute, U+00F3 ISOlat1
    {"ocirc",    0x00f4}, // latin small letter o with circumflex, U+00F4 ISOlat1
    {"oelig",    0x0153}, // latin small ligature oe, U+0153 ISOlat2
    {"ograve",   0x00f2}, // latin small letter o with grave, U+00F2 ISOlat1
    {"oline",    0x203e}, // overline = spacing overscore, U+203E NEW
    {"omega",    0x03c9}, // greek small letter omega, U+03C9 ISOgrk3
    {"omicron",  0x03bf}, // greek small letter omicron, U+03BF NEW
    {"oplus",    0x2295}, // circled plus = direct sum, U+2295 ISOamsb
    {"or",       0x2228}, // logical or = vee, U+2228 ISOtech
    {"ordf",     0x00aa}, // feminine ordinal indicator, U+00AA ISOnum
    {"ordm",     0x00ba}, // masculine ordinal indicator, U+00BA ISOnum
    {"oslash",   0x00f8}, // latin small letter o with stroke, = latin small letter o slash, U+00F8 ISOlat1
    {"otilde",   0x00f5}, // latin small letter o with tilde, U+00F5 ISOlat1
    {"otimes",   0x2297}, // circled times = vector product, U+2297 ISOamsb
    {"ouml",     0x00f6}, // latin small letter o with diaeresis, U+00F6 ISOlat1
    {"para",     0x00b6}, // pilcrow sign = paragraph sign, U+00B6 ISOnum
    {"part",     0x2202}, // partial differential, U+2202 ISOtech
    {"permil",   0x2030}, // per mille sign, U+2030 ISOtech
    {"perp",     0x22a5}, // up tack = orthogonal to = perpendicular, U+22A5 ISOtech
    {"phi",      0x03c6}, // greek small letter phi, U+03C6 ISOgrk3
    {"pi",       0x03c0}, // greek small letter pi, U+03C0 ISOgrk3
    {"piv",      0x03d6}, // greek pi symbol, U+03D6 ISOgrk3
    {"plusmn",   0x00b1}, // plus-minus sign = plus-or-minus sign, U+00B1 ISOnum
    {"pound",    0x00a3}, // pound sign, U+00A3 ISOnum
    {"prime",    0x2032}, // prime = minutes = feet, U+2032 ISOtech
    {"prod",     0x220f}, // n-ary product = product sign, U+220F ISOamsb
    {"prop",     0x221d}, // proportional to, U+221D ISOtech
    {"psi",      0x03c8}, // greek small letter psi, U+03C8 ISOgrk3
    {"quot",     0x0022}, // quotation mark = APL quote, U+0022 ISOnum
    {"rArr",     0x21d2}, // rightwards double arrow, U+21D2 ISOtech
    {"radic",    0x221a}, // square root = radical sign, U+221A ISOtech
    {"rang",     0x232a}, // right-pointing angle bracket = ket, U+232A ISOtech
    {"raquo",    0x00bb}, // right-pointing double angle quotation mark = right pointing guillemet, U+00BB ISOnum
    {"rarr",     0x2192}, // rightwards arrow, U+2192 ISOnum
    {"rceil",    0x2309}, // right ceiling, U+2309 ISOamsc
    {"rdquo",    0x201d}, // right double quotation mark, U+201D ISOnum
    {"real",     0x211c}, // blackletter capital R = real part symbol, U+211C ISOamso
    {"reg",      0x00ae}, // registered sign = registered trade mark sign, U+00AE ISOnum
    {"rfloor",   0x230b}, // right floor, U+230B ISOamsc
    {"rho",      0x03c1}, // greek small letter rho, U+03C1 ISOgrk3
    {"rlm",      0x200f}, // right-to-left mark, U+200F NEW RFC 2070
    {"rsaquo",   0x203a}, // single right-pointing angle quotation mark, U+203A ISO proposed
    {"rsquo",    0x2019}, // right single quotation mark, U+2019 ISOnum
    {"sbquo",    0x201a}, // single low-9 quotation mark, U+201A NEW
    {"scaron",   0x0161}, // latin small letter s with caron, U+0161 ISOlat2
    {"sdot",     0x22c5}, // dot operator, U+22C5 ISOamsb
    {"sect",     0x00a7}, // section sign, U+00A7 ISOnum
    {"shy",      0x00ad}, // soft hyphen = discretionary hyphen, U+00AD ISOnum
    {"sigma",    0x03c3}, // greek small letter sigma, U+03C3 ISOgrk3
    {"sigmaf",   0x03c2}, // greek small letter final sigma, U+03C2 ISOgrk3
    {"sim",      0x223c}, // tilde operator = varies with = similar to, U+223C ISOtech
    {"spades",   0x2660}, // black spade suit, U+2660 ISOpub
    {"sub",      0x2282}, // subset of, U+2282 ISOtech
    {"sube",     0x2286}, // subset of or equal to, U+2286 ISOtech
    {"sum",      0x2211}, // n-ary sumation, U+2211 ISOamsb
    {"sup",      0x2283}, // superset of, U+2283 ISOtech
    {"sup1",     0x00b9}, // superscript one = superscript digit one, U+00B9 ISOnum
    {"sup2",     0x00b2}, // superscript two = superscript digit two = squared, U+00B2 ISOnum
    {"sup3",     0x00b3}, // superscript three = superscript digit three = cubed, U+00B3 ISOnum
    {"supe",     0x2287}, // superset of or equal to, U+2287 ISOtech
    {"szlig",    0x00df}, // latin small letter sharp s = ess-zed, U+00DF ISOlat1
    {"tau",      0x03c4}, // greek small letter tau, U+03C4 ISOgrk3
    {"there4",   0x2234}, // therefore, U+2234 ISOtech
    {"theta",    0x03b8}, // greek small letter theta, U+03B8 ISOgrk3
    {"thetasym", 0x03d1}, // greek small letter theta symbol, U+03D1 NEW
    {"thinsp",   0x2009}, // thin space, U+2009 ISOpub
    {"thorn",    0x00fe}, // latin small letter thorn with, U+00FE ISOlat1
    {"tilde",    0x02dc}, // small tilde, U+02DC ISOdia
    {"times",    0x00d7}, // multiplication sign, U+00D7 ISOnum
    {"trade",    0x2122}, // trade mark sign, U+2122 ISOnum
    {"uArr",     0x21d1}, // upwards double arrow, U+21D1 ISOamsa
    {"uacute",   0x00fa}, // latin small letter u with acute, U+00FA ISOlat1
    {"uarr",     0x2191}, // upwards arrow, U+2191 ISOnum
    {"ucirc",    0x00fb}, // latin small letter u with circumflex, U+00FB ISOlat1
    {"ugrave",   0x00f9}, // latin small letter u with grave, U+00F9 ISOlat1
    {"uml",      0x00a8}, // diaeresis = spacing diaeresis, U+00A8 ISOdia
    {"upsih",    0x03d2}, // greek upsilon with hook symbol, U+03D2 NEW
    {"upsilon",  0x03c5}, // greek small letter upsilon, U+03C5 ISOgrk3
    {"uuml",     0x00fc}, // latin small letter u with diaeresis, U+00FC ISOlat1
    {"weierp",   0x2118}, // script capital P = power set = Weierstrass p, U+2118 ISOamso
    {"xi",       0x03be}, // greek small letter xi, U+03BE ISOgrk3
    {"yacute",   0x00fd}, // latin small letter y with acute, U+00FD ISOlat1
    {"yen",      0x00a5}, // yen sign = yuan sign, U+00A5 ISOnum
    {"yuml",     0x00ff}, // latin small letter y with diaeresis, U+00FF ISOlat1
    {"zeta",     0x03b6}, // greek small letter zeta, U+03B6 ISOgrk3
    {"zwj",      0x200d}, // zero width joiner, U+200D NEW RFC 2070
    {"zwnj",     0x200c}, // zero width non-joiner, U+200C NEW RFC 2070

    {"apos",     0x0027}  // HTMLParser supports apos, which is not part of HTML 4
};

const QStringList CDATA_CONTENT_ELEMENTS {"script", "style"};

// Regular expressions used for parsing

const QString INTERESTING_NORMAL("[&<]");
const QString INCOMPLETE("&[a-zA-Z#]");

const QString ENTITY_REF("&([a-zA-Z][-.a-zA-Z0-9]*)[^a-zA-Z0-9]");
const QString CHAR_REF("&#(?:[0-9]+|[xX][0-9a-fA-F]+)[^0-9a-fA-F]");

const QString STARTTAG_OPEN("<[a-zA-Z]");
const QString PI_CLOSE(">");
const QString COMMENT_CLOSE(R"(--\s*>)");

// see http://www.w3.org/TR/html5/tokenization.html#tag-open-state
// and http://www.w3.org/TR/html5/tokenization.html#tag-name-state
// note: if you change tagfind/attrfind remember to update locatestarttagend too
const QString TAG_FIND(R"(([a-zA-Z][^\t\n\r\f />\x00]*)(?:\s|/(?!>))*)");

// Original ATTR_FIND and LOCATE_STARTTAG_END use Positive Lookbehind
// but QRegExp doesn't support it. These expressions work without it in
// most cases so we just drop it.
// Original expressions can be used as is if we will use QRegularExpression instead
// of QRegExp but it requires Qt5.
const QString ATTR_FIND(
    R"(([^\s/>][^\s/=>]*)(\s*=+\s*)"
    R"((\'[^\']*\'|"[^"]*"|(?![\'"])[^>\s]*))?(?:\s|\/(?!>))*)");

const QString LOCATE_STARTTAG_END(
    R"(<[a-zA-Z][^\t\n\r\f />\x00]*)"           // tag name
    R"((?:[\s/]*)"                              // optional whitespace before attribute name
        R"((?:[^\s/>][^\s/=>]*)"                // attribute name
            R"((?:\s*=+\s*)"                    // value indicator
                R"((?:'[^']*')"                 // LITA-enclosed value
                    R"(|"[^"]*")"               // LIT-enclosed value
                    R"(|(?!['"])[^>\s]*)"       // bare value
                R"())"
            R"()?(?:\s|\/(?!>))*)"
        R"()*)"
    R"()?)"
    R"(\s*)");                                   // trailing whitespace

// const QString ATTR_FIND(
//     R"(((?<=[\'"\s/])[^\s/>][^\s/=>]*)(\s*=+\s*)"
//     R"((\'[^\']*\'|"[^"]*"|(?![\'"])[^>\s]*))?(?:\s|\/(?!>))*)");
//
// const QString LOCATE_STARTTAG_END(
//     R"(<[a-zA-Z][^\t\n\r\f />\x00]*)"           // tag name
//     R"((?:[\s/]*)"                              // optional whitespace before attribute name
//         R"((?:(?<=['"\s/])[^\s/>][^\s/=>]*)"    // attribute name
//             R"((?:\s*=+\s*)"                    // value indicator
//                 R"((?:'[^']*')"                 // LITA-enclosed value
//                     R"(|"[^"]*")"               // LIT-enclosed value
//                     R"(|(?!['"])[^>\s]*)"       // bare value
//                 R"())"
//             R"()?(?:\s|\/(?!>))*)"
//         R"()*)"
//     R"()?)"
//     R"(\s*)");                                   // trailing whitespace

const QString END_ENDTAG(">");

// the HTML 5 spec, section 8.1.2.2, doesn't allow spaces between
// </ and the tag name, so maybe this should be fixed
const QString ENDTAG_FIND(R"(</\s*([a-zA-Z][-.a-zA-Z0-9:_]*)\s*>)");

const QString DECL_NAME(R"([a-zA-Z][-_.a-zA-Z0-9]*\s*)");
const QString MARKEDSECTION_CLOSE(R"(]\s*]\s*>)");

// An analysis of the MS-Word extensions is available at
// http://www.planetpublish.com/xmlarena/xap/Thursday/WordtoXML.pdf
const QString MSMARKEDSECTION_CLOSE(R"(]\s*>)");

// Initialize and reset this instance.
HTMLParser::HTMLParser()
    : m_rxInterestingNormal {INTERESTING_NORMAL}
    , m_rxIncomplete {INCOMPLETE}
    , m_rxEntityRef {ENTITY_REF}
    , m_rxCharRef {CHAR_REF}
    , m_rxStarttagOpen {STARTTAG_OPEN}
    , m_rxPiClose {PI_CLOSE}
    , m_rxCommentClose {COMMENT_CLOSE}
    , m_rxTagFind {TAG_FIND}
    , m_rxAttrFind {ATTR_FIND}
    , m_rxLocateStarttagEnd {LOCATE_STARTTAG_END}
    , m_rxEndEndtag {END_ENDTAG}
    , m_rxEndtagFind {ENDTAG_FIND}
    , m_rxDeclName {DECL_NAME}
    , m_rxMarkedSectionClose {MARKEDSECTION_CLOSE}
    , m_rxMsMarkedSectionClose {MSMARKEDSECTION_CLOSE}
    , m_decoder {}
{
    reset();
}

HTMLParser::~HTMLParser() {}

// Reset this instance. Loses all unprocessed data.
void HTMLParser::reset()
{
    m_rawData.clear();
    m_lastTag = "???";
    m_interesting = m_rxInterestingNormal;
    m_cdataElem.clear();
    m_lineNo = 1;
    m_offset = 0;
    delete m_decoder;
    m_decoder = nullptr;
}

// Feed data to the parser.
// Call this as often as you want, with as little or as much text
// as you want (may include '\n').
void HTMLParser::feed(const QByteArray &data)
{
    if (!m_decoder)
        m_decoder = QTextCodec::codecForHtml(data)->makeDecoder();

    m_rawData.append(m_decoder->toUnicode(data));
    goAhead(false);
}

// Handle any buffered data.
void HTMLParser::close()
{
    goAhead(true);
    reset();
}

void HTMLParser::handleStartEndTag(const QString &tag, const QHash<QString, QString> &attrs)
{
    handleStartTag(tag, attrs);
    handleEndTag(tag);
}

void HTMLParser::handleEntityRef(const QString &name)
{
   handleData(entityToChar(name));
}

void HTMLParser::handleCharRef(const QString &name)
{
    int c;
    if ((name[0] == 'x') || (name[0] == 'X'))
        c = name.mid(1).toInt(nullptr, 16);
    else
        c = name.toInt();
    handleData(QChar(c));
}

void HTMLParser::handleStartTag(const QString &, const QHash<QString, QString> &) {}
void HTMLParser::handleEndTag(const QString &) {}
void HTMLParser::handleData(const QString &) {}
void HTMLParser::handleComment(const QString &) {}
void HTMLParser::handleDecl(const QString &) {}
void HTMLParser::handlePI(const QString &) {}
void HTMLParser::handleUnknownDecl(const QString &) {}

void HTMLParser::error(const QString &message)
{
    throw HTMLParseError(message, m_lineNo, m_offset);
}

void HTMLParser::setCDATAMode(const QString &elem)
{
    m_cdataElem = elem.toLower();
    m_interesting = QRegExp(QString(R"(</\s*%1\s*>)").arg(m_cdataElem), Qt::CaseInsensitive);
}

void HTMLParser::clearCDATAMode()
{
    m_cdataElem.clear();
    m_interesting = m_rxInterestingNormal;
}

// Internal -- update line number and offset.  This should be
// called for each piece of data exactly once, in order -- in other
// words the concatenation of all the input strings to this
// function should be exactly the entire input.
int HTMLParser::updatePos(int i, int j)
{
    if (i >= j)
        return j;

    QString rawData = m_rawData.mid(i, j - i);
    int nlines = rawData.count("\n");
    if (nlines > 0) {
        m_lineNo += nlines;
        int pos = rawData.lastIndexOf("\n"); // Should not fail
        m_offset = j - (pos + 1);
    }
    else {
        m_offset += j - i;
    }

    return j;
}

// Internal -- handle data as far as reasonable.  May leave state
// and data to be processed by a subsequent call.  If 'end' is
// true, force handling all data as if followed by EOF marker.
void HTMLParser::goAhead(bool end)
{
    QString rawData = m_rawData;
    int i = 0;
    int n = rawData.size();

    while (i < n)
    {
        int j = m_interesting.indexIn(rawData, i); // < or &
        if (j == -1) {
            if (!m_cdataElem.isEmpty())
                break;
            j = n;
        }

        if (i < j)
            handleData(rawData.mid(i, j - i));

        i = updatePos(i, j);
        if (i == n) break;

        QStringRef currentRawDataRef = rawData.midRef(i);
        if (currentRawDataRef.startsWith('<')) {
            int k;
            if (m_rxStarttagOpen.indexIn(rawData, i) == i) {
                // < + letter
                k = parseStartTag(i);
            }
            else if (currentRawDataRef.startsWith("</")) {
                k = parseEndTag(i);
            }
            else if (currentRawDataRef.startsWith("<!--")) {
                k = parseComment(i);
            }
            else if (currentRawDataRef.startsWith("<?")) {
                k = parsePI(i);
            }
            else if (currentRawDataRef.startsWith("<!")) {
                k = parseHTMLDeclaration(i);
            }
            else if ((i + 1) < n) {
                handleData("<");
                k = i + 1;
            }
            else {
                break;
            }

            if (k < 0) {
                if (!end) break;

                k = rawData.indexOf('>', i + 1);
                if (k < 0) {
                    k = rawData.indexOf('<', i + 1);
                    if (k < 0)
                        k = i + 1;
                }
                else {
                    k += 1;
                }

                handleData(rawData.mid(i, k - i));
            }

            i = updatePos(i, k);
        }
        else if (currentRawDataRef.startsWith("&#")) {
            int pos = m_rxCharRef.indexIn(rawData, i);
            if (pos == i) {
                QString name = m_rxCharRef.cap(0).mid(2, m_rxCharRef.cap(0).size() - 3);
                handleCharRef(name);
                int k = pos + m_rxCharRef.cap(0).size();
                if (rawData[k - 1] != ';')
                    --k;
                i = updatePos(i, k);
                continue;
            }
            else {
                if (currentRawDataRef.contains(";")) {
                    // bail by consuming '&#'
                    handleData(rawData.mid(i, 2));
                    i = updatePos(i, i + 2);
                }
                break;
            }
        }
        else if (currentRawDataRef.startsWith('&')) {
            int pos = m_rxEntityRef.indexIn(rawData, i);
            if (pos == i) {
                QString name = m_rxEntityRef.cap(1);
                handleEntityRef(name);
                int k = pos + m_rxEntityRef.cap(0).size();
                if (rawData[k - 1] != ';')
                    --k;
                i = updatePos(i, k);
                continue;
            }

            pos = m_rxIncomplete.indexIn(rawData, i);
            if (pos == i) {
                // INCOMPLETE.cap(0) will contain at least 2 chars
                if (end && (m_rxIncomplete.cap(0) == currentRawDataRef))
                    error("EOF in middle of entity or char ref");
                // incomplete
                break;
            }
            else if ((i + 1) < n) {
                // not the end of the buffer, and can't be confused
                // with some other construct
                handleData("&");
                i = updatePos(i, i + 1);
            }
            else {
                break;
            }
        }
        else {
            Q_ASSERT_X(false, Q_FUNC_INFO, "INTERESTING.indexIn() lied");
        }
    }

    if (end && (i < n) && m_cdataElem.isEmpty()) {
        handleData(rawData.mid(i, n - i));
        i = updatePos(i, n);
    }

    m_rawData = rawData.mid(i);
}

// Internal -- parse html declarations, return length or -1 if not terminated
// See w3.org/TR/html5/tokenization.html#markup-declaration-open-state
int HTMLParser::parseHTMLDeclaration(int i)
{
    QString rawData = m_rawData;
    if (rawData.midRef(i, 2) != "<!")
        error("unexpected call to parseHTMLDeclaration()");
    if (rawData.midRef(i, 2) == "<!--") {
        // this case is actually already handled in goAhead()
        return parseComment(i);
    }
    else if (rawData.midRef(i, 3) == "<![") {
        return parseMarkedSection(i);
    }
    else if (rawData.mid(i, 9).toLower() == "<!doctype") {
        // find the closing >
        int gtpos = rawData.indexOf('>', i + 9);
        if (gtpos == -1)
            return -1;
        i += 2;
        handleDecl(rawData.mid(i, gtpos - i));
        return gtpos + 1;
    }
    else {
        return parseBogusComment(i);
    }
}

// Internal -- parse bogus comment, return length or -1 if not terminated
// see http://www.w3.org/TR/html5/tokenization.html#bogus-comment-state
int HTMLParser::parseBogusComment(int i, bool report)
{
    QString rawData = m_rawData;
    if ((rawData.midRef(i, 2) != "<!") && (rawData.midRef(i, 2) != "</"))
        error("unexpected call to parseBogusComment()");
    i += 2;
    int pos = rawData.indexOf('>', i);
    if (pos == -1)
        return -1;
    if (report)
        handleComment(rawData.mid(i, pos - i));
    return pos + 1;
}

// Internal -- parse processing instr, return end or -1 if not terminated
int HTMLParser::parsePI(int i)
{
    QString rawData = m_rawData;
    Q_ASSERT_X(rawData.midRef(i, 2) == "<?", Q_FUNC_INFO, "unexpected call to parsePI()");
    i += 2;
    int pos = m_rxPiClose.indexIn(rawData, i); // >
    if (pos == -1)
        return -1;
    handlePI(rawData.mid(i, pos - i));
    return pos + 1;
}

// Internal -- handle starttag, return end or -1 if not terminated
int HTMLParser::parseStartTag(int i)
{
    m_startTagText.clear();
    int endpos = checkForWholeStartTag(i);
    if (endpos < 0) return endpos;

    QString rawData = m_rawData;
    m_startTagText = rawData.mid(i, endpos - i);

    // Now parse the data between i+1 and j into a tag and attrs
    QHash<QString, QString> attrs;
    int pos = m_rxTagFind.indexIn(rawData, i + 1);
    Q_ASSERT_X(pos == i + 1, Q_FUNC_INFO, "unexpected call to parseStartTag()");
    int k = pos + m_rxTagFind.cap(0).size();
    m_lastTag = m_rxTagFind.cap(1).toLower();
    while (k < endpos)
    {
        int m = m_rxAttrFind.indexIn(rawData, k);
        if (m != k) break;

        QString attrname = m_rxAttrFind.cap(1);
        QString attrvalue = m_rxAttrFind.cap(3);
        if (!attrvalue.isEmpty()) {
            if (((attrvalue.startsWith('\'') && attrvalue.endsWith('\''))
                 || (attrvalue.startsWith('"') && attrvalue.endsWith('"'))))
                attrvalue = attrvalue.mid(1, attrvalue.size() - 2);
        }
        if (!attrvalue.isEmpty())
            attrvalue = unescape(attrvalue);

        attrs[attrname.toLower()] = attrvalue;
        k = m + m_rxAttrFind.cap(0).size();
    }

    QString end = rawData.mid(k, endpos - k);
    if ((end != ">") && (end != "/>"))
    {
        int lineno = m_lineNo;
        int offset = m_offset;
        if (m_startTagText.contains("\n"))
        {
            lineno += m_startTagText.count("\n");
            offset = m_startTagText.size() - m_startTagText.lastIndexOf("\n");
        }
        else {
            offset += m_startTagText.size();
        }

        handleData(rawData.mid(i, endpos - i));
        return endpos;
    }

    if (end.endsWith("/>")) {
        // XHTML-style empty tag: <span attr="value" />
        handleStartEndTag(m_lastTag, attrs);
    }
    else {
        handleStartTag(m_lastTag, attrs);
        if (CDATA_CONTENT_ELEMENTS.contains(m_lastTag))
            setCDATAMode(m_lastTag);
    }

    return endpos;
}

// Internal -- check to see if we have a complete starttag; return end
// or -1 if incomplete.
int HTMLParser::checkForWholeStartTag(int i)
{
    QString rawData = m_rawData;
    int m = m_rxLocateStarttagEnd.indexIn(rawData, i);
    if (m == i) {
        int j = i + m_rxLocateStarttagEnd.cap(0).size();
        QString next = rawData.mid(j, 1);
        if (next == ">") return j + 1;

        if (next == "/") {
            if (rawData.midRef(j).startsWith("/>"))
                return j + 2;
            if (rawData.midRef(j).startsWith("/"))
                // buffer boundary
                return -1;
            // else bogus input
            updatePos(i, j + 1);
            error("malformed empty start tag");
        }

        if (next == "")
            // end of input
            return -1;
        if (QString("abcdefghijklmnopqrstuvwxyz=/ABCDEFGHIJKLMNOPQRSTUVWXYZ").contains(next))
            // end of input in or before attribute value, or we have the
            // '/' from a '/>' ending
            return -1;
        if (j > i)
            return j;
        else
            return i + 1;
    }

    Q_ASSERT_X(false, Q_FUNC_INFO, "we should not get here!");
    return -1;
}

// Internal -- parse endtag, return end or -1 if incomplete
int HTMLParser::parseEndTag(int i)
{
    QString rawData = m_rawData;
    Q_ASSERT_X(rawData.midRef(i, 2) == "</", Q_FUNC_INFO, "unexpected call to parseEndTag()");
    int pos = m_rxEndEndtag.indexIn(rawData, i + 1); // >
    if (pos == -1) return -1;

    int gtpos = pos + m_rxEndEndtag.cap(0).size();
    pos = m_rxEndtagFind.indexIn(rawData, i); // </ + tag + >
    if (pos != i) {
        if (!m_cdataElem.isEmpty()) {
            handleData(rawData.mid(i, gtpos - i));
            return gtpos;
        }

        // find the name: w3.org/TR/html5/tokenization.html#tag-name-state
        int namepos = m_rxTagFind.indexIn(rawData, i + 2);
        if (namepos != (i + 2)) {
            // w3.org/TR/html5/tokenization.html#end-tag-open-state
            if (rawData.midRef(i, 3) == "</>")
                return i + 3;
            else
                return parseBogusComment(i);
        }

        QString tagname = m_rxTagFind.cap(1).toLower();
        // consume and ignore other stuff between the name and the >
        // Note: this is not 100% correct, since we might have things like
        // </tag attr=">">, but looking for > after tha name should cover
        // most of the cases and is much simpler
        gtpos = rawData.indexOf('>', namepos + m_rxTagFind.cap(0).size());
        handleEndTag(tagname);
        return gtpos + 1;
    }

    QString elem = m_rxEndtagFind.cap(1).toLower(); // script or style
    if (!m_cdataElem.isEmpty()) {
        if (elem != m_cdataElem) {
            handleData(rawData.mid(i, gtpos - i));
            return gtpos;
        }
    }

    handleEndTag(elem);
    clearCDATAMode();
    return gtpos;
}

// Internal -- parse a marked section
// Override this to handle MS-word extension syntax <![if word]>content<![endif]>
int HTMLParser::parseMarkedSection(int i, bool report)
{
    QString rawData = m_rawData;
    Q_ASSERT_X(rawData.midRef(i, 3) == "<![", Q_FUNC_INFO, "unexpected call to parseMarkedSection()");

    QString sectName;
    int j = scanName(i + 3, i, sectName);
    if (j < 0) return j;

    i += 3;
    int pos, endpos;
    if (QSet<QString>({"temp", "cdata", "ignore", "include", "rcdata"}).contains(sectName)) {
        // look for standard ]]> ending
        pos = m_rxMarkedSectionClose.indexIn(rawData, i);
        endpos = pos + m_rxMarkedSectionClose.cap(0).size();
    }
    else if (QSet<QString>({"if", "else", "endif"}).contains(sectName)) {
        // look for MS Office ]> ending
        pos = m_rxMsMarkedSectionClose.indexIn(rawData, i);
        endpos = pos + m_rxMsMarkedSectionClose.cap(0).size();
    }
    else {
        error(QString("unknown status keyword %1 in marked section").arg(rawData.mid(i, j - i)));
    }

    if (pos == -1) return -1;

    if (report)
        handleUnknownDecl(rawData.mid(i, pos - i));
    return endpos;
}

// Internal -- parse comment, return length or -1 if not terminated
int HTMLParser::parseComment(int i, bool report)
{
    QString rawData = m_rawData;
    if (rawData.midRef(i, 4) != "<!--")
        error("unexpected call to parseComment()");

    int pos = m_rxCommentClose.indexIn(rawData, i + 4);
    if (pos == -1) return -1;

    if (report)
        handleComment(rawData.mid(i, pos - i));

    return pos + m_rxCommentClose.cap(0).size();
}

int HTMLParser::scanName(int i, int declstartpos, QString &name)
{
    QString rawData = m_rawData;
    int n = rawData.size();
    if (i == n) return -1;

    int pos = m_rxDeclName.indexIn(rawData, i);
    if (pos == i) {
        QString s = m_rxDeclName.cap(0);
        name = s.trimmed().toLower();
        if ((i + s.size()) == n)
            return -1; // end of buffer

        return pos + s.size();
    }
    else {
        updatePos(declstartpos, i);
        error(QString("expected name token at %1").arg(rawData.mid(declstartpos, 20)));
        return -1;
    }
}

QChar HTMLParser::entityToChar(const QString &entityName)
{
    return QChar(NAME2CODEPOINT.value(entityName, 0));
}

// Helper to remove special character quoting
QString HTMLParser::unescape(QString str)
{
    if (!str.contains('&')) return str;

    QRegExp re(R"(&(#?[xX]?(?:[0-9a-fA-F]+|\w{1,8}));)");
    int pos = 0;
    while ((pos = re.indexIn(str, pos)) != -1) {
        QString s = re.cap(1);
        int size = re.cap(0).size();

        int c;
        if (s[0] == '#') {
            s = s.mid(1);
            if ((s[0] == 'x') || (s[0] == 'X'))
                c = s.mid(1).toInt(nullptr, 16);
            else
                c = s.toInt();
            str.replace(pos, size, QChar(c));
        }
        else {
            c = NAME2CODEPOINT.value(s, 0);
            if (c != 0)
                str.replace(pos, size, QChar(c));
        }

        ++pos;
    }

    return str;
}

HTMLParseError::HTMLParseError(const QString &msg, int lineNo, int offset)
    : m_msg(msg)
    , m_lineNo(lineNo)
    , m_offset(offset)
{
    Q_ASSERT(!msg.isEmpty());
}

QString HTMLParseError::message() const
{
    QString result = m_msg;
    if (m_lineNo >= 0)
        result.append(QString(", at line %1").arg(m_lineNo));
    if (m_offset >= 0)
        result.append(QString(", column %1").arg(m_offset + 1));
    return result;
}

int HTMLParseError::lineNo() const
{
    return m_lineNo;
}

int HTMLParseError::offset() const
{
    return m_offset;
}
