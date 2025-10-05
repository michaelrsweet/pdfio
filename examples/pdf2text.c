//
// PDF to text program for PDFio.
//
// Copyright © 2022-2025 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Usage:
//
//   ./pdf2text FILENAME.pdf > FILENAME.txt
//

#include <pdfio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>


//
// Mapping table for character names to Unicode values.
//

typedef struct name_map_s
{
  const char	*name;			// Character name
  int		unicode;		// Unicode value
} name_map_t;

static name_map_t	unicode_map[] =
{
  { "A",		0x0041 },
  { "AE",		0x00c6 },
  { "AEacute",		0x01fc },
  { "AEsmall",		0xf7e6 },
  { "Aacute",		0x00c1 },
  { "Aacutesmall",	0xf7e1 },
  { "Abreve",		0x0102 },
  { "Acircumflex",	0x00c2 },
  { "Acircumflexsmall",	0xf7e2 },
  { "Acute",		0xf6c9 },
  { "Acutesmall",	0xf7b4 },
  { "Adieresis",	0x00c4 },
  { "Adieresissmall",	0xf7e4 },
  { "Agrave",		0x00c0 },
  { "Agravesmall",	0xf7e0 },
  { "Alpha",		0x0391 },
  { "Alphatonos",	0x0386 },
  { "Amacron",		0x0100 },
  { "Aogonek",		0x0104 },
  { "Aring",		0x00c5 },
  { "Aringacute",	0x01fa },
  { "Aringsmall",	0xf7e5 },
  { "Asmall",		0xf761 },
  { "Atilde",		0x00c3 },
  { "Atildesmall",	0xf7e3 },
  { "B",		0x0042 },
  { "Beta",		0x0392 },
  { "Brevesmall",	0xf6f4 },
  { "Bsmall",		0xf762 },
  { "C",		0x0043 },
  { "Cacute",		0x0106 },
  { "Caron",		0xf6ca },
  { "Caronsmall",	0xf6f5 },
  { "Ccaron",		0x010c },
  { "Ccedilla",		0x00c7 },
  { "Ccedillasmall",	0xf7e7 },
  { "Ccircumflex",	0x0108 },
  { "Cdotaccent",	0x010a },
  { "Cedillasmall",	0xf7b8 },
  { "Chi",		0x03a7 },
  { "Circumflexsmall",	0xf6f6 },
  { "Csmall",		0xf763 },
  { "D",		0x0044 },
  { "Dcaron",		0x010e },
  { "Dcroat",		0x0110 },
  { "Delta",		0x0394 },
  { "Delta",		0x2206 },
  { "Dieresis",		0xf6cb },
  { "DieresisAcute",	0xf6cc },
  { "DieresisGrave",	0xf6cd },
  { "Dieresissmall",	0xf7a8 },
  { "Dotaccentsmall",	0xf6f7 },
  { "Dsmall",		0xf764 },
  { "E",		0x0045 },
  { "Eacute",		0x00c9 },
  { "Eacutesmall",	0xf7e9 },
  { "Ebreve",		0x0114 },
  { "Ecaron",		0x011a },
  { "Ecircumflex",	0x00ca },
  { "Ecircumflexsmall",	0xf7ea },
  { "Edieresis",	0x00cb },
  { "Edieresissmall",	0xf7eb },
  { "Edotaccent",	0x0116 },
  { "Egrave",		0x00c8 },
  { "Egravesmall",	0xf7e8 },
  { "Emacron",		0x0112 },
  { "Eng",		0x014a },
  { "Eogonek",		0x0118 },
  { "Epsilon",		0x0395 },
  { "Epsilontonos",	0x0388 },
  { "Esmall",		0xf765 },
  { "Eta",		0x0397 },
  { "Etatonos",		0x0389 },
  { "Eth",		0x00d0 },
  { "Ethsmall",		0xf7f0 },
  { "Euro",		0x20ac },
  { "F",		0x0046 },
  { "Fsmall",		0xf766 },
  { "G",		0x0047 },
  { "Gamma",		0x0393 },
  { "Gbreve",		0x011e },
  { "Gcaron",		0x01e6 },
  { "Gcircumflex",	0x011c },
  { "Gcommaaccent",	0x0122 },
  { "Gdotaccent",	0x0120 },
  { "Grave",		0xf6ce },
  { "Gravesmall",	0xf760 },
  { "Gsmall",		0xf767 },
  { "H",		0x0048 },
  { "H18533",		0x25cf },
  { "H18543",		0x25aa },
  { "H18551",		0x25ab },
  { "H22073",		0x25a1 },
  { "Hbar",		0x0126 },
  { "Hcircumflex",	0x0124 },
  { "Hsmall",		0xf768 },
  { "Hungarumlaut",	0xf6cf },
  { "Hungarumlautsmall", 0xf6f8 },
  { "I",		0x0049 },
  { "IJ",		0x0132 },
  { "Iacute",		0x00cd },
  { "Iacutesmall",	0xf7ed },
  { "Ibreve",		0x012c },
  { "Icircumflex",	0x00ce },
  { "Icircumflexsmall",	0xf7ee },
  { "Idieresis",	0x00cf },
  { "Idieresissmall",	0xf7ef },
  { "Idotaccent",	0x0130 },
  { "Ifraktur",		0x2111 },
  { "Igrave",		0x00cc },
  { "Igravesmall",	0xf7ec },
  { "Imacron",		0x012a },
  { "Iogonek",		0x012e },
  { "Iota",		0x0399 },
  { "Iotadieresis",	0x03aa },
  { "Iotatonos",	0x038a },
  { "Ismall",		0xf769 },
  { "Itilde",		0x0128 },
  { "J",		0x004a },
  { "Jcircumflex",	0x0134 },
  { "Jsmall",		0xf76a },
  { "K",		0x004b },
  { "Kappa",		0x039a },
  { "Kcommaaccent",	0x0136 },
  { "Ksmall",		0xf76b },
  { "L",		0x004c },
  { "LL",		0xf6bf },
  { "Lacute",		0x0139 },
  { "Lambda",		0x039b },
  { "Lcaron",		0x013d },
  { "Lcommaaccent",	0x013b },
  { "Ldot",		0x013f },
  { "Lslash",		0x0141 },
  { "Lslashsmall",	0xf6f9 },
  { "Lsmall",		0xf76c },
  { "M",		0x004d },
  { "Macron",		0xf6d0 },
  { "Macronsmall",	0xf7af },
  { "Msmall",		0xf76d },
  { "Mu",		0x039c },
  { "N",		0x004e },
  { "Nacute",		0x0143 },
  { "Ncaron",		0x0147 },
  { "Ncommaaccent",	0x0145 },
  { "Nsmall",		0xf76e },
  { "Ntilde",		0x00d1 },
  { "Ntildesmall",	0xf7f1 },
  { "Nu",		0x039d },
  { "O",		0x004f },
  { "OE",		0x0152 },
  { "OEsmall",		0xf6fa },
  { "Oacute",		0x00d3 },
  { "Oacutesmall",	0xf7f3 },
  { "Obreve",		0x014e },
  { "Ocircumflex",	0x00d4 },
  { "Ocircumflexsmall",	0xf7f4 },
  { "Odieresis",	0x00d6 },
  { "Odieresissmall",	0xf7f6 },
  { "Ogoneksmall",	0xf6fb },
  { "Ograve",		0x00d2 },
  { "Ogravesmall",	0xf7f2 },
  { "Ohorn",		0x01a0 },
  { "Ohungarumlaut",	0x0150 },
  { "Omacron",		0x014c },
  { "Omega",		0x03a9 },
  { "Omega",		0x2126 },
  { "Omegatonos",	0x038f },
  { "Omicron",		0x039f },
  { "Omicrontonos",	0x038c },
  { "Oslash",		0x00d8 },
  { "Oslashacute",	0x01fe },
  { "Oslashsmall",	0xf7f8 },
  { "Osmall",		0xf76f },
  { "Otilde",		0x00d5 },
  { "Otildesmall",	0xf7f5 },
  { "P",		0x0050 },
  { "Phi",		0x03a6 },
  { "Pi",		0x03a0 },
  { "Psi",		0x03a8 },
  { "Psmall",		0xf770 },
  { "Q",		0x0051 },
  { "Qsmall",		0xf771 },
  { "R",		0x0052 },
  { "Racute",		0x0154 },
  { "Rcaron",		0x0158 },
  { "Rcommaaccent",	0x0156 },
  { "Rfraktur",		0x211c },
  { "Rho",		0x03a1 },
  { "Ringsmall",	0xf6fc },
  { "Rsmall",		0xf772 },
  { "S",		0x0053 },
  { "SF010000",		0x250c },
  { "SF020000",		0x2514 },
  { "SF030000",		0x2510 },
  { "SF040000",		0x2518 },
  { "SF050000",		0x253c },
  { "SF060000",		0x252c },
  { "SF070000",		0x2534 },
  { "SF080000",		0x251c },
  { "SF090000",		0x2524 },
  { "SF100000",		0x2500 },
  { "SF110000",		0x2502 },
  { "SF190000",		0x2561 },
  { "SF200000",		0x2562 },
  { "SF210000",		0x2556 },
  { "SF220000",		0x2555 },
  { "SF230000",		0x2563 },
  { "SF240000",		0x2551 },
  { "SF250000",		0x2557 },
  { "SF260000",		0x255d },
  { "SF270000",		0x255c },
  { "SF280000",		0x255b },
  { "SF360000",		0x255e },
  { "SF370000",		0x255f },
  { "SF380000",		0x255a },
  { "SF390000",		0x2554 },
  { "SF400000",		0x2569 },
  { "SF410000",		0x2566 },
  { "SF420000",		0x2560 },
  { "SF430000",		0x2550 },
  { "SF440000",		0x256c },
  { "SF450000",		0x2567 },
  { "SF460000",		0x2568 },
  { "SF470000",		0x2564 },
  { "SF480000",		0x2565 },
  { "SF490000",		0x2559 },
  { "SF500000",		0x2558 },
  { "SF510000",		0x2552 },
  { "SF520000",		0x2553 },
  { "SF530000",		0x256b },
  { "SF540000",		0x256a },
  { "Sacute",		0x015a },
  { "Scaron",		0x0160 },
  { "Scaronsmall",	0xf6fd },
  { "Scedilla",		0x015e },
  { "Scedilla",		0xf6c1 },
  { "Scircumflex",	0x015c },
  { "Scommaaccent",	0x0218 },
  { "Sigma",		0x03a3 },
  { "Ssmall",		0xf773 },
  { "T",		0x0054 },
  { "Tau",		0x03a4 },
  { "Tbar",		0x0166 },
  { "Tcaron",		0x0164 },
  { "Tcommaaccent",	0x0162 },
  { "Tcommaaccent",	0x021a },
  { "Theta",		0x0398 },
  { "Thorn",		0x00de },
  { "Thornsmall",	0xf7fe },
  { "Tildesmall",	0xf6fe },
  { "Tsmall",		0xf774 },
  { "U",		0x0055 },
  { "Uacute",		0x00da },
  { "Uacutesmall",	0xf7fa },
  { "Ubreve",		0x016c },
  { "Ucircumflex",	0x00db },
  { "Ucircumflexsmall",	0xf7fb },
  { "Udieresis",	0x00dc },
  { "Udieresissmall",	0xf7fc },
  { "Ugrave",		0x00d9 },
  { "Ugravesmall",	0xf7f9 },
  { "Uhorn",		0x01af },
  { "Uhungarumlaut",	0x0170 },
  { "Umacron",		0x016a },
  { "Uogonek",		0x0172 },
  { "Upsilon",		0x03a5 },
  { "Upsilon1",		0x03d2 },
  { "Upsilondieresis",	0x03ab },
  { "Upsilontonos",	0x038e },
  { "Uring",		0x016e },
  { "Usmall",		0xf775 },
  { "Utilde",		0x0168 },
  { "V",		0x0056 },
  { "Vsmall",		0xf776 },
  { "W",		0x0057 },
  { "Wacute",		0x1e82 },
  { "Wcircumflex",	0x0174 },
  { "Wdieresis",	0x1e84 },
  { "Wgrave",		0x1e80 },
  { "Wsmall",		0xf777 },
  { "X",		0x0058 },
  { "Xi",		0x039e },
  { "Xsmall",		0xf778 },
  { "Y",		0x0059 },
  { "Yacute",		0x00dd },
  { "Yacutesmall",	0xf7fd },
  { "Ycircumflex",	0x0176 },
  { "Ydieresis",	0x0178 },
  { "Ydieresissmall",	0xf7ff },
  { "Ygrave",		0x1ef2 },
  { "Ysmall",		0xf779 },
  { "Z",		0x005a },
  { "Zacute",		0x0179 },
  { "Zcaron",		0x017d },
  { "Zcaronsmall",	0xf6ff },
  { "Zdotaccent",	0x017b },
  { "Zeta",		0x0396 },
  { "Zsmall",		0xf77a },
  { "a",		0x0061 },
  { "aacute",		0x00e1 },
  { "abreve",		0x0103 },
  { "acircumflex",	0x00e2 },
  { "acute",		0x00b4 },
  { "acutecomb",	0x0301 },
  { "adieresis",	0x00e4 },
  { "ae",		0x00e6 },
  { "aeacute",		0x01fd },
  { "afii00208",	0x2015 },
  { "afii10017",	0x0410 },
  { "afii10018",	0x0411 },
  { "afii10019",	0x0412 },
  { "afii10020",	0x0413 },
  { "afii10021",	0x0414 },
  { "afii10022",	0x0415 },
  { "afii10023",	0x0401 },
  { "afii10024",	0x0416 },
  { "afii10025",	0x0417 },
  { "afii10026",	0x0418 },
  { "afii10027",	0x0419 },
  { "afii10028",	0x041a },
  { "afii10029",	0x041b },
  { "afii10030",	0x041c },
  { "afii10031",	0x041d },
  { "afii10032",	0x041e },
  { "afii10033",	0x041f },
  { "afii10034",	0x0420 },
  { "afii10035",	0x0421 },
  { "afii10036",	0x0422 },
  { "afii10037",	0x0423 },
  { "afii10038",	0x0424 },
  { "afii10039",	0x0425 },
  { "afii10040",	0x0426 },
  { "afii10041",	0x0427 },
  { "afii10042",	0x0428 },
  { "afii10043",	0x0429 },
  { "afii10044",	0x042a },
  { "afii10045",	0x042b },
  { "afii10046",	0x042c },
  { "afii10047",	0x042d },
  { "afii10048",	0x042e },
  { "afii10049",	0x042f },
  { "afii10050",	0x0490 },
  { "afii10051",	0x0402 },
  { "afii10052",	0x0403 },
  { "afii10053",	0x0404 },
  { "afii10054",	0x0405 },
  { "afii10055",	0x0406 },
  { "afii10056",	0x0407 },
  { "afii10057",	0x0408 },
  { "afii10058",	0x0409 },
  { "afii10059",	0x040a },
  { "afii10060",	0x040b },
  { "afii10061",	0x040c },
  { "afii10062",	0x040e },
  { "afii10063",	0xf6c4 },
  { "afii10064",	0xf6c5 },
  { "afii10065",	0x0430 },
  { "afii10066",	0x0431 },
  { "afii10067",	0x0432 },
  { "afii10068",	0x0433 },
  { "afii10069",	0x0434 },
  { "afii10070",	0x0435 },
  { "afii10071",	0x0451 },
  { "afii10072",	0x0436 },
  { "afii10073",	0x0437 },
  { "afii10074",	0x0438 },
  { "afii10075",	0x0439 },
  { "afii10076",	0x043a },
  { "afii10077",	0x043b },
  { "afii10078",	0x043c },
  { "afii10079",	0x043d },
  { "afii10080",	0x043e },
  { "afii10081",	0x043f },
  { "afii10082",	0x0440 },
  { "afii10083",	0x0441 },
  { "afii10084",	0x0442 },
  { "afii10085",	0x0443 },
  { "afii10086",	0x0444 },
  { "afii10087",	0x0445 },
  { "afii10088",	0x0446 },
  { "afii10089",	0x0447 },
  { "afii10090",	0x0448 },
  { "afii10091",	0x0449 },
  { "afii10092",	0x044a },
  { "afii10093",	0x044b },
  { "afii10094",	0x044c },
  { "afii10095",	0x044d },
  { "afii10096",	0x044e },
  { "afii10097",	0x044f },
  { "afii10098",	0x0491 },
  { "afii10099",	0x0452 },
  { "afii10100",	0x0453 },
  { "afii10101",	0x0454 },
  { "afii10102",	0x0455 },
  { "afii10103",	0x0456 },
  { "afii10104",	0x0457 },
  { "afii10105",	0x0458 },
  { "afii10106",	0x0459 },
  { "afii10107",	0x045a },
  { "afii10108",	0x045b },
  { "afii10109",	0x045c },
  { "afii10110",	0x045e },
  { "afii10145",	0x040f },
  { "afii10146",	0x0462 },
  { "afii10147",	0x0472 },
  { "afii10148",	0x0474 },
  { "afii10192",	0xf6c6 },
  { "afii10193",	0x045f },
  { "afii10194",	0x0463 },
  { "afii10195",	0x0473 },
  { "afii10196",	0x0475 },
  { "afii10831",	0xf6c7 },
  { "afii10832",	0xf6c8 },
  { "afii10846",	0x04d9 },
  { "afii299",		0x200e },
  { "afii300",		0x200f },
  { "afii301",		0x200d },
  { "afii57381",	0x066a },
  { "afii57388",	0x060c },
  { "afii57392",	0x0660 },
  { "afii57393",	0x0661 },
  { "afii57394",	0x0662 },
  { "afii57395",	0x0663 },
  { "afii57396",	0x0664 },
  { "afii57397",	0x0665 },
  { "afii57398",	0x0666 },
  { "afii57399",	0x0667 },
  { "afii57400",	0x0668 },
  { "afii57401",	0x0669 },
  { "afii57403",	0x061b },
  { "afii57407",	0x061f },
  { "afii57409",	0x0621 },
  { "afii57410",	0x0622 },
  { "afii57411",	0x0623 },
  { "afii57412",	0x0624 },
  { "afii57413",	0x0625 },
  { "afii57414",	0x0626 },
  { "afii57415",	0x0627 },
  { "afii57416",	0x0628 },
  { "afii57417",	0x0629 },
  { "afii57418",	0x062a },
  { "afii57419",	0x062b },
  { "afii57420",	0x062c },
  { "afii57421",	0x062d },
  { "afii57422",	0x062e },
  { "afii57423",	0x062f },
  { "afii57424",	0x0630 },
  { "afii57425",	0x0631 },
  { "afii57426",	0x0632 },
  { "afii57427",	0x0633 },
  { "afii57428",	0x0634 },
  { "afii57429",	0x0635 },
  { "afii57430",	0x0636 },
  { "afii57431",	0x0637 },
  { "afii57432",	0x0638 },
  { "afii57433",	0x0639 },
  { "afii57434",	0x063a },
  { "afii57440",	0x0640 },
  { "afii57441",	0x0641 },
  { "afii57442",	0x0642 },
  { "afii57443",	0x0643 },
  { "afii57444",	0x0644 },
  { "afii57445",	0x0645 },
  { "afii57446",	0x0646 },
  { "afii57448",	0x0648 },
  { "afii57449",	0x0649 },
  { "afii57450",	0x064a },
  { "afii57451",	0x064b },
  { "afii57452",	0x064c },
  { "afii57453",	0x064d },
  { "afii57454",	0x064e },
  { "afii57455",	0x064f },
  { "afii57456",	0x0650 },
  { "afii57457",	0x0651 },
  { "afii57458",	0x0652 },
  { "afii57470",	0x0647 },
  { "afii57505",	0x06a4 },
  { "afii57506",	0x067e },
  { "afii57507",	0x0686 },
  { "afii57508",	0x0698 },
  { "afii57509",	0x06af },
  { "afii57511",	0x0679 },
  { "afii57512",	0x0688 },
  { "afii57513",	0x0691 },
  { "afii57514",	0x06ba },
  { "afii57519",	0x06d2 },
  { "afii57534",	0x06d5 },
  { "afii57636",	0x20aa },
  { "afii57645",	0x05be },
  { "afii57658",	0x05c3 },
  { "afii57664",	0x05d0 },
  { "afii57665",	0x05d1 },
  { "afii57666",	0x05d2 },
  { "afii57667",	0x05d3 },
  { "afii57668",	0x05d4 },
  { "afii57669",	0x05d5 },
  { "afii57670",	0x05d6 },
  { "afii57671",	0x05d7 },
  { "afii57672",	0x05d8 },
  { "afii57673",	0x05d9 },
  { "afii57674",	0x05da },
  { "afii57675",	0x05db },
  { "afii57676",	0x05dc },
  { "afii57677",	0x05dd },
  { "afii57678",	0x05de },
  { "afii57679",	0x05df },
  { "afii57680",	0x05e0 },
  { "afii57681",	0x05e1 },
  { "afii57682",	0x05e2 },
  { "afii57683",	0x05e3 },
  { "afii57684",	0x05e4 },
  { "afii57685",	0x05e5 },
  { "afii57686",	0x05e6 },
  { "afii57687",	0x05e7 },
  { "afii57688",	0x05e8 },
  { "afii57689",	0x05e9 },
  { "afii57690",	0x05ea },
  { "afii57694",	0xfb2a },
  { "afii57695",	0xfb2b },
  { "afii57700",	0xfb4b },
  { "afii57705",	0xfb1f },
  { "afii57716",	0x05f0 },
  { "afii57717",	0x05f1 },
  { "afii57718",	0x05f2 },
  { "afii57723",	0xfb35 },
  { "afii57793",	0x05b4 },
  { "afii57794",	0x05b5 },
  { "afii57795",	0x05b6 },
  { "afii57796",	0x05bb },
  { "afii57797",	0x05b8 },
  { "afii57798",	0x05b7 },
  { "afii57799",	0x05b0 },
  { "afii57800",	0x05b2 },
  { "afii57801",	0x05b1 },
  { "afii57802",	0x05b3 },
  { "afii57803",	0x05c2 },
  { "afii57804",	0x05c1 },
  { "afii57806",	0x05b9 },
  { "afii57807",	0x05bc },
  { "afii57839",	0x05bd },
  { "afii57841",	0x05bf },
  { "afii57842",	0x05c0 },
  { "afii57929",	0x02bc },
  { "afii61248",	0x2105 },
  { "afii61289",	0x2113 },
  { "afii61352",	0x2116 },
  { "afii61573",	0x202c },
  { "afii61574",	0x202d },
  { "afii61575",	0x202e },
  { "afii61664",	0x200c },
  { "afii63167",	0x066d },
  { "afii64937",	0x02bd },
  { "agrave",		0x00e0 },
  { "aleph",		0x2135 },
  { "alpha",		0x03b1 },
  { "alphatonos",	0x03ac },
  { "amacron",		0x0101 },
  { "ampersand",	0x0026 },
  { "ampersandsmall",	0xf726 },
  { "angle",		0x2220 },
  { "angleleft",	0x2329 },
  { "angleright",	0x232a },
  { "anoteleia",	0x0387 },
  { "aogonek",		0x0105 },
  { "approxequal",	0x2248 },
  { "aring",		0x00e5 },
  { "aringacute",	0x01fb },
  { "arrowboth",	0x2194 },
  { "arrowdblboth",	0x21d4 },
  { "arrowdbldown",	0x21d3 },
  { "arrowdblleft",	0x21d0 },
  { "arrowdblright",	0x21d2 },
  { "arrowdblup",	0x21d1 },
  { "arrowdown",	0x2193 },
  { "arrowhorizex",	0xf8e7 },
  { "arrowleft",	0x2190 },
  { "arrowright",	0x2192 },
  { "arrowup",		0x2191 },
  { "arrowupdn",	0x2195 },
  { "arrowupdnbse",	0x21a8 },
  { "arrowvertex",	0xf8e6 },
  { "asciicircum",	0x005e },
  { "asciitilde",	0x007e },
  { "asterisk",		0x002a },
  { "asteriskmath",	0x2217 },
  { "asuperior",	0xf6e9 },
  { "at",		0x0040 },
  { "atilde",		0x00e3 },
  { "b",		0x0062 },
  { "backslash",	0x005c },
  { "bar",		0x007c },
  { "beta",		0x03b2 },
  { "block",		0x2588 },
  { "braceex",		0xf8f4 },
  { "braceleft",	0x007b },
  { "braceleftbt",	0xf8f3 },
  { "braceleftmid",	0xf8f2 },
  { "bracelefttp",	0xf8f1 },
  { "braceright",	0x007d },
  { "bracerightbt",	0xf8fe },
  { "bracerightmid",	0xf8fd },
  { "bracerighttp",	0xf8fc },
  { "bracketleft",	0x005b },
  { "bracketleftbt",	0xf8f0 },
  { "bracketleftex",	0xf8ef },
  { "bracketlefttp",	0xf8ee },
  { "bracketright",	0x005d },
  { "bracketrightbt",	0xf8fb },
  { "bracketrightex",	0xf8fa },
  { "bracketrighttp",	0xf8f9 },
  { "breve",		0x02d8 },
  { "brokenbar",	0x00a6 },
  { "bsuperior",	0xf6ea },
  { "bullet",		0x2022 },
  { "c",		0x0063 },
  { "cacute",		0x0107 },
  { "caron",		0x02c7 },
  { "carriagereturn",	0x21b5 },
  { "ccaron",		0x010d },
  { "ccedilla",		0x00e7 },
  { "ccircumflex",	0x0109 },
  { "cdotaccent",	0x010b },
  { "cedilla",		0x00b8 },
  { "cent",		0x00a2 },
  { "centinferior",	0xf6df },
  { "centoldstyle",	0xf7a2 },
  { "centsuperior",	0xf6e0 },
  { "chi",		0x03c7 },
  { "circle",		0x25cb },
  { "circlemultiply",	0x2297 },
  { "circleplus",	0x2295 },
  { "circumflex",	0x02c6 },
  { "club",		0x2663 },
  { "colon",		0x003a },
  { "colonmonetary",	0x20a1 },
  { "comma",		0x002c },
  { "commaaccent",	0xf6c3 },
  { "commainferior",	0xf6e1 },
  { "commasuperior",	0xf6e2 },
  { "congruent",	0x2245 },
  { "copyright",	0x00a9 },
  { "copyrightsans",	0xf8e9 },
  { "copyrightserif",	0xf6d9 },
  { "currency",		0x00a4 },
  { "cyrBreve",		0xf6d1 },
  { "cyrFlex",		0xf6d2 },
  { "cyrbreve",		0xf6d4 },
  { "cyrflex",		0xf6d5 },
  { "d",		0x0064 },
  { "dagger",		0x2020 },
  { "daggerdbl",	0x2021 },
  { "dblGrave",		0xf6d3 },
  { "dblgrave",		0xf6d6 },
  { "dcaron",		0x010f },
  { "dcroat",		0x0111 },
  { "degree",		0x00b0 },
  { "delta",		0x03b4 },
  { "diamond",		0x2666 },
  { "dieresis",		0x00a8 },
  { "dieresisacute",	0xf6d7 },
  { "dieresisgrave",	0xf6d8 },
  { "dieresistonos",	0x0385 },
  { "divide",		0x00f7 },
  { "dkshade",		0x2593 },
  { "dnblock",		0x2584 },
  { "dollar",		0x0024 },
  { "dollarinferior",	0xf6e3 },
  { "dollaroldstyle",	0xf724 },
  { "dollarsuperior",	0xf6e4 },
  { "dong",		0x20ab },
  { "dotaccent",	0x02d9 },
  { "dotbelowcomb",	0x0323 },
  { "dotlessi",		0x0131 },
  { "dotlessj",		0xf6be },
  { "dotmath",		0x22c5 },
  { "dsuperior",	0xf6eb },
  { "e",		0x0065 },
  { "eacute",		0x00e9 },
  { "ebreve",		0x0115 },
  { "ecaron",		0x011b },
  { "ecircumflex",	0x00ea },
  { "edieresis",	0x00eb },
  { "edotaccent",	0x0117 },
  { "egrave",		0x00e8 },
  { "eight",		0x0038 },
  { "eightinferior",	0x2088 },
  { "eightoldstyle",	0xf738 },
  { "eightsuperior",	0x2078 },
  { "element",		0x2208 },
  { "ellipsis",		0x2026 },
  { "emacron",		0x0113 },
  { "emdash",		0x2014 },
  { "emptyset",		0x2205 },
  { "endash",		0x2013 },
  { "eng",		0x014b },
  { "eogonek",		0x0119 },
  { "epsilon",		0x03b5 },
  { "epsilontonos",	0x03ad },
  { "equal",		0x003d },
  { "equivalence",	0x2261 },
  { "estimated",	0x212e },
  { "esuperior",	0xf6ec },
  { "eta",		0x03b7 },
  { "etatonos",		0x03ae },
  { "eth",		0x00f0 },
  { "exclam",		0x0021 },
  { "exclamdbl",	0x203c },
  { "exclamdown",	0x00a1 },
  { "exclamdownsmall",	0xf7a1 },
  { "exclamsmall",	0xf721 },
  { "existential",	0x2203 },
  { "f",		0x0066 },
  { "female",		0x2640 },
  { "ff",		0xfb00 },
  { "ffi",		0xfb03 },
  { "ffl",		0xfb04 },
  { "fi",		0xfb01 },
  { "figuredash",	0x2012 },
  { "filledbox",	0x25a0 },
  { "filledrect",	0x25ac },
  { "five",		0x0035 },
  { "fiveeighths",	0x215d },
  { "fiveinferior",	0x2085 },
  { "fiveoldstyle",	0xf735 },
  { "fivesuperior",	0x2075 },
  { "fl",		0xfb02 },
  { "florin",		0x0192 },
  { "four",		0x0034 },
  { "fourinferior",	0x2084 },
  { "fouroldstyle",	0xf734 },
  { "foursuperior",	0x2074 },
  { "fraction",		0x2044 },
  { "fraction",		0x2215 },
  { "franc",		0x20a3 },
  { "g",		0x0067 },
  { "gamma",		0x03b3 },
  { "gbreve",		0x011f },
  { "gcaron",		0x01e7 },
  { "gcircumflex",	0x011d },
  { "gcommaaccent",	0x0123 },
  { "gdotaccent",	0x0121 },
  { "germandbls",	0x00df },
  { "gradient",		0x2207 },
  { "grave",		0x0060 },
  { "gravecomb",	0x0300 },
  { "greater",		0x003e },
  { "greaterequal",	0x2265 },
  { "guillemotleft",	0x00ab },
  { "guillemotright",	0x00bb },
  { "guilsinglleft",	0x2039 },
  { "guilsinglright",	0x203a },
  { "h",		0x0068 },
  { "hbar",		0x0127 },
  { "hcircumflex",	0x0125 },
  { "heart",		0x2665 },
  { "hookabovecomb",	0x0309 },
  { "house",		0x2302 },
  { "hungarumlaut",	0x02dd },
  { "hyphen",		0x002d },
  { "hypheninferior",	0xf6e5 },
  { "hyphensuperior",	0xf6e6 },
  { "i",		0x0069 },
  { "iacute",		0x00ed },
  { "ibreve",		0x012d },
  { "icircumflex",	0x00ee },
  { "idieresis",	0x00ef },
  { "igrave",		0x00ec },
  { "ij",		0x0133 },
  { "imacron",		0x012b },
  { "infinity",		0x221e },
  { "integral",		0x222b },
  { "integralbt",	0x2321 },
  { "integralex",	0xf8f5 },
  { "integraltp",	0x2320 },
  { "intersection",	0x2229 },
  { "invbullet",	0x25d8 },
  { "invcircle",	0x25d9 },
  { "invsmileface",	0x263b },
  { "iogonek",		0x012f },
  { "iota",		0x03b9 },
  { "iotadieresis",	0x03ca },
  { "iotadieresistonos", 0x0390 },
  { "iotatonos",	0x03af },
  { "isuperior",	0xf6ed },
  { "itilde",		0x0129 },
  { "j",		0x006a },
  { "jcircumflex",	0x0135 },
  { "k",		0x006b },
  { "kappa",		0x03ba },
  { "kcommaaccent",	0x0137 },
  { "kgreenlandic",	0x0138 },
  { "l",		0x006c },
  { "lacute",		0x013a },
  { "lambda",		0x03bb },
  { "lcaron",		0x013e },
  { "lcommaaccent",	0x013c },
  { "ldot",		0x0140 },
  { "less",		0x003c },
  { "lessequal",	0x2264 },
  { "lfblock",		0x258c },
  { "lira",		0x20a4 },
  { "ll",		0xf6c0 },
  { "logicaland",	0x2227 },
  { "logicalnot",	0x00ac },
  { "logicalor",	0x2228 },
  { "longs",		0x017f },
  { "lozenge",		0x25ca },
  { "lslash",		0x0142 },
  { "lsuperior",	0xf6ee },
  { "ltshade",		0x2591 },
  { "m",		0x006d },
  { "macron",		0x00af },
  { "macron",		0x02c9 },
  { "male",		0x2642 },
  { "minus",		0x00ad },
  { "minus",		0x2212 },
  { "minute",		0x2032 },
  { "msuperior",	0xf6ef },
  { "mu",		0x00b5 },
  { "mu",		0x03bc },
  { "multiply",		0x00d7 },
  { "musicalnote",	0x266a },
  { "musicalnotedbl",	0x266b },
  { "n",		0x006e },
  { "nacute",		0x0144 },
  { "napostrophe",	0x0149 },
  { "ncaron",		0x0148 },
  { "ncommaaccent",	0x0146 },
  { "nine",		0x0039 },
  { "nineinferior",	0x2089 },
  { "nineoldstyle",	0xf739 },
  { "ninesuperior",	0x2079 },
  { "notelement",	0x2209 },
  { "notequal",		0x2260 },
  { "notsubset",	0x2284 },
  { "nsuperior",	0x207f },
  { "ntilde",		0x00f1 },
  { "nu",		0x03bd },
  { "numbersign",	0x0023 },
  { "o",		0x006f },
  { "oacute",		0x00f3 },
  { "obreve",		0x014f },
  { "ocircumflex",	0x00f4 },
  { "odieresis",	0x00f6 },
  { "oe",		0x0153 },
  { "ogonek",		0x02db },
  { "ograve",		0x00f2 },
  { "ohorn",		0x01a1 },
  { "ohungarumlaut",	0x0151 },
  { "omacron",		0x014d },
  { "omega",		0x03c9 },
  { "omega1",		0x03d6 },
  { "omegatonos",	0x03ce },
  { "omicron",		0x03bf },
  { "omicrontonos",	0x03cc },
  { "one",		0x0031 },
  { "onedotenleader",	0x2024 },
  { "oneeighth",	0x215b },
  { "onefitted",	0xf6dc },
  { "onehalf",		0x00bd },
  { "oneinferior",	0x2081 },
  { "oneoldstyle",	0xf731 },
  { "onequarter",	0x00bc },
  { "onesuperior",	0x00b9 },
  { "onethird",		0x2153 },
  { "openbullet",	0x25e6 },
  { "ordfeminine",	0x00aa },
  { "ordmasculine",	0x00ba },
  { "orthogonal",	0x221f },
  { "oslash",		0x00f8 },
  { "oslashacute",	0x01ff },
  { "osuperior",	0xf6f0 },
  { "otilde",		0x00f5 },
  { "p",		0x0070 },
  { "paragraph",	0x00b6 },
  { "parenleft",	0x0028 },
  { "parenleftbt",	0xf8ed },
  { "parenleftex",	0xf8ec },
  { "parenleftinferior", 0x208d },
  { "parenleftsuperior", 0x207d },
  { "parenlefttp",	0xf8eb },
  { "parenright",	0x0029 },
  { "parenrightbt",	0xf8f8 },
  { "parenrightex",	0xf8f7 },
  { "parenrightinferior", 0x208e },
  { "parenrightsuperior", 0x207e },
  { "parenrighttp",	0xf8f6 },
  { "partialdiff",	0x2202 },
  { "percent",		0x0025 },
  { "period",		0x002e },
  { "periodcentered",	0x00b7 },
  { "periodcentered",	0x2219 },
  { "periodinferior",	0xf6e7 },
  { "periodsuperior",	0xf6e8 },
  { "perpendicular",	0x22a5 },
  { "perthousand",	0x2030 },
  { "peseta",		0x20a7 },
  { "phi",		0x03c6 },
  { "phi1",		0x03d5 },
  { "pi",		0x03c0 },
  { "plus",		0x002b },
  { "plusminus",	0x00b1 },
  { "prescription",	0x211e },
  { "product",		0x220f },
  { "propersubset",	0x2282 },
  { "propersuperset",	0x2283 },
  { "proportional",	0x221d },
  { "psi",		0x03c8 },
  { "q",		0x0071 },
  { "question",		0x003f },
  { "questiondown",	0x00bf },
  { "questiondownsmall", 0xf7bf },
  { "questionsmall",	0xf73f },
  { "quotedbl",		0x0022 },
  { "quotedblbase",	0x201e },
  { "quotedblleft",	0x201c },
  { "quotedblright",	0x201d },
  { "quoteleft",	0x2018 },
  { "quotereversed",	0x201b },
  { "quoteright",	0x2019 },
  { "quotesinglbase",	0x201a },
  { "quotesingle",	0x0027 },
  { "r",		0x0072 },
  { "racute",		0x0155 },
  { "radical",		0x221a },
  { "radicalex",	0xf8e5 },
  { "rcaron",		0x0159 },
  { "rcommaaccent",	0x0157 },
  { "reflexsubset",	0x2286 },
  { "reflexsuperset",	0x2287 },
  { "registered",	0x00ae },
  { "registersans",	0xf8e8 },
  { "registerserif",	0xf6da },
  { "revlogicalnot",	0x2310 },
  { "rho",		0x03c1 },
  { "ring",		0x02da },
  { "rsuperior",	0xf6f1 },
  { "rtblock",		0x2590 },
  { "rupiah",		0xf6dd },
  { "s",		0x0073 },
  { "sacute",		0x015b },
  { "scaron",		0x0161 },
  { "scedilla",		0x015f },
  { "scedilla",		0xf6c2 },
  { "scircumflex",	0x015d },
  { "scommaaccent",	0x0219 },
  { "second",		0x2033 },
  { "section",		0x00a7 },
  { "semicolon",	0x003b },
  { "seven",		0x0037 },
  { "seveneighths",	0x215e },
  { "seveninferior",	0x2087 },
  { "sevenoldstyle",	0xf737 },
  { "sevensuperior",	0x2077 },
  { "shade",		0x2592 },
  { "sigma",		0x03c3 },
  { "sigma1",		0x03c2 },
  { "similar",		0x223c },
  { "six",		0x0036 },
  { "sixinferior",	0x2086 },
  { "sixoldstyle",	0xf736 },
  { "sixsuperior",	0x2076 },
  { "slash",		0x002f },
  { "smileface",	0x263a },
  { "space",		0x0020 },
  { "space",		0x00a0 },
  { "spade",		0x2660 },
  { "ssuperior",	0xf6f2 },
  { "sterling",		0x00a3 },
  { "suchthat",		0x220b },
  { "summation",	0x2211 },
  { "sun",		0x263c },
  { "t",		0x0074 },
  { "tau",		0x03c4 },
  { "tbar",		0x0167 },
  { "tcaron",		0x0165 },
  { "tcommaaccent",	0x0163 },
  { "tcommaaccent",	0x021b },
  { "therefore",	0x2234 },
  { "theta",		0x03b8 },
  { "theta1",		0x03d1 },
  { "thorn",		0x00fe },
  { "three",		0x0033 },
  { "threeeighths",	0x215c },
  { "threeinferior",	0x2083 },
  { "threeoldstyle",	0xf733 },
  { "threequarters",	0x00be },
  { "threequartersemdash", 0xf6de },
  { "threesuperior",	0x00b3 },
  { "tilde",		0x02dc },
  { "tildecomb",	0x0303 },
  { "tonos",		0x0384 },
  { "trademark",	0x2122 },
  { "trademarksans",	0xf8ea },
  { "trademarkserif",	0xf6db },
  { "triagdn",		0x25bc },
  { "triaglf",		0x25c4 },
  { "triagrt",		0x25ba },
  { "triagup",		0x25b2 },
  { "tsuperior",	0xf6f3 },
  { "two",		0x0032 },
  { "twodotenleader",	0x2025 },
  { "twoinferior",	0x2082 },
  { "twooldstyle",	0xf732 },
  { "twosuperior",	0x00b2 },
  { "twothirds",	0x2154 },
  { "u",		0x0075 },
  { "uacute",		0x00fa },
  { "ubreve",		0x016d },
  { "ucircumflex",	0x00fb },
  { "udieresis",	0x00fc },
  { "ugrave",		0x00f9 },
  { "uhorn",		0x01b0 },
  { "uhungarumlaut",	0x0171 },
  { "umacron",		0x016b },
  { "underscore",	0x005f },
  { "underscoredbl",	0x2017 },
  { "union",		0x222a },
  { "universal",	0x2200 },
  { "uogonek",		0x0173 },
  { "upblock",		0x2580 },
  { "upsilon",		0x03c5 },
  { "upsilondieresis",	0x03cb },
  { "upsilondieresistonos", 0x03b0 },
  { "upsilontonos",	0x03cd },
  { "uring",		0x016f },
  { "utilde",		0x0169 },
  { "v",		0x0076 },
  { "w",		0x0077 },
  { "wacute",		0x1e83 },
  { "wcircumflex",	0x0175 },
  { "wdieresis",	0x1e85 },
  { "weierstrass",	0x2118 },
  { "wgrave",		0x1e81 },
  { "x",		0x0078 },
  { "xi",		0x03be },
  { "y",		0x0079 },
  { "yacute",		0x00fd },
  { "ycircumflex",	0x0177 },
  { "ydieresis",	0x00ff },
  { "yen",		0x00a5 },
  { "ygrave",		0x1ef3 },
  { "z",		0x007a },
  { "zacute",		0x017a },
  { "zcaron",		0x017e },
  { "zdotaccent",	0x017c },
  { "zero",		0x0030 },
  { "zeroinferior",	0x2080 },
  { "zerooldstyle",	0xf730 },
  { "zerosuperior",	0x2070 },
  { "zeta",		0x03b6 }
};


//
// Local functions...
//

static void	load_encoding(pdfio_obj_t *page_obj, const char *name, int encoding[256]);
static void	put_utf8(int ch);
static void	puts_utf16(const char *s);


//
// 'main()' - Main entry.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  pdfio_file_t	*file;			// PDF file
  size_t	i, j,			// Looping vars
		num_pages,		// Number of pages
		num_streams;		// Number of streams for page
  pdfio_obj_t	*obj;			// Current page object
  pdfio_stream_t *st;			// Current page content stream
  char		buffer[1024],		// String buffer
		*bufptr,		// Pointer into buffer
		name[256];		// Current (font) name
  bool		first;			// First string token?
  int		encoding[256];		// Font encoding to Unicode
  bool		in_array = false;	// Are we in an array?


  // Verify command-line arguments...
  if (argc != 2)
  {
    puts("Usage: pdf2text FILENAME.pdf > FILENAME.txt");
    return (1);
  }

  // Open the PDF file...
  if ((file = pdfioFileOpen(argv[1], /*password_cb*/NULL, /*password_data*/NULL, /*error_cb*/NULL, /*error_data*/NULL)) == NULL)
    return (1);

  // Try grabbing content from all of the pages...
  for (i = 0, num_pages = pdfioFileGetNumPages(file); i < num_pages; i ++)
  {
    if ((obj = pdfioFileGetPage(file, i)) == NULL)
      continue;

    load_encoding(obj, "", encoding);
    name[0] = '\0';

    num_streams = pdfioPageGetNumStreams(obj);

    for (j = 0; j < num_streams; j ++)
    {
      if ((st = pdfioPageOpenStream(obj, j, true)) == NULL)
	continue;

      // Read PDF tokens from the page stream...
      first = true;
      while (pdfioStreamGetToken(st, buffer, sizeof(buffer)))
      {
        if (!strcmp(buffer, "["))
        {
          // Start of an array for justified text...
          in_array = true;
        }
        else if (!strcmp(buffer, "]"))
        {
          // End of an array for justified text...
          in_array = false;
        }
        else if (!first && in_array && (isdigit(buffer[0]) || buffer[0] == '-') && fabs(atof(buffer)) > 100)
        {
          // Whitespace in a justified text block...
          putchar(' ');
        }
	else if (buffer[0] == '(')
	{
          // Text string using an 8-bit encoding
	  first = false;

          for (bufptr = buffer + 1; *bufptr; bufptr ++)
            put_utf8(encoding[*bufptr & 255]);
	}
	else if (buffer[0] == '<')
	{
          // Unicode text string
	  first = false;

          puts_utf16(buffer + 1);
	}
	else if (buffer[0] == '/')
	{
	  // Save name...
	  strncpy(name, buffer + 1, sizeof(name) - 1);
	  name[sizeof(name) - 1] = '\0';
	}
	else if (!strcmp(buffer, "Tf") && name[0])
	{
	  // Set font...
	  load_encoding(obj, name, encoding);
	}
	else if (!first && (!strcmp(buffer, "Td") || !strcmp(buffer, "TD") || !strcmp(buffer, "T*") || !strcmp(buffer, "\'") || !strcmp(buffer, "\"")))
	{
	  // Text operators that advance to the next line in the block
	  putchar('\n');
	  first = true;
	}
      }

      if (!first)
	putchar('\n');

      pdfioStreamClose(st);
    }
  }

  pdfioFileClose(file);

  return (0);
}


//
// 'load_encoding()' - Load the encoding for a font.
//

static void
load_encoding(
    pdfio_obj_t   *page_obj,		// I - Page object
    const char    *name,		// I - Font name
    int           encoding[256])	// O - Encoding table
{
  size_t	i, j;			// Looping vars
  pdfio_dict_t	*page_dict,		// Page dictionary
		*resources_dict,	// Resources dictionary
		*font_dict;		// Font dictionary
  pdfio_obj_t	*font_obj,		// Font object
		*encoding_obj;		// Encoding object
  pdfio_dict_t	*encoding_dict;		// Encoding dictionary
  const char	*base_encoding;		// BaseEncoding name
  pdfio_array_t	*differences;		// Differences array
  static int	win_ansi[32] =		// WinANSI characters from 128 to 159
  {
    0x20AC, 0x0000, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x0000, 0x017D, 0x0000,
    0x0000, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x0000, 0x017E, 0x0178
  };
  static int	mac_roman[128] =	// MacRoman characters from 128 to 255
  {
    0x00C4, 0x00C5, 0x00C7, 0x00C9, 0x00D1, 0x00D6, 0x00DC, 0x00E1,
    0x00E0, 0x00E2, 0x00E4, 0x00E3, 0x00E5, 0x00E7, 0x00E9, 0x00E8,
    0x00EA, 0x00EB, 0x00ED, 0x00EC, 0x00EE, 0x00EF, 0x00F1, 0x00F3,
    0x00F2, 0x00F4, 0x00F6, 0x00F5, 0x00FA, 0x00F9, 0x00FB, 0x00FC,

    0x2020, 0x00B0, 0x00A2, 0x00A3, 0x00A7, 0x2022, 0x00B6, 0x00DF,
    0x00AE, 0x00A9, 0x2122, 0x00B4, 0x00A8, 0x2260, 0x00C6, 0x00D8,
    0x221E, 0x00B1, 0x2264, 0x2265, 0x00A5, 0x00B5, 0x2202, 0x2211,
    0x220F, 0x03C0, 0x222B, 0x00AA, 0x00BA, 0x03A9, 0x00E6, 0x00F8,

    0x00BF, 0x00A1, 0x00AC, 0x221A, 0x0192, 0x2248, 0x2206, 0x00AB,
    0x00BB, 0x2026, 0x00A0, 0x00C0, 0x00C3, 0x00D5, 0x0152, 0x0153,
    0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0x00F7, 0x25CA,
    0x00FF, 0x0178, 0x2044, 0x20AC, 0x2039, 0x203A, 0xFB01, 0xFB02,

    0x2021, 0x00B7, 0x201A, 0x201E, 0x2030, 0x00C2, 0x00CA, 0x00C1,
    0x00CB, 0x00C8, 0x00CD, 0x00CE, 0x00CF, 0x00CC, 0x00D3, 0x00D4,
    0xF8FF, 0x00D2, 0x00DA, 0x00DB, 0x00D9, 0x0131, 0x02C6, 0x02DC,
    0x00AF, 0x02D8, 0x02D9, 0x02DA, 0x00B8, 0x02DD, 0x02DB, 0x02C7
  };
  // TODO: Add MacExpert encoding...


  // Initialize the encoding to be the "standard" WinAnsi...
  for (i = 0; i < 128; i ++)
    encoding[i] = i;
  for (i = 160; i < 256; i ++)
    encoding[i] = i;
  memcpy(encoding + 128, win_ansi, sizeof(win_ansi));

  // Find the named font...
  if ((page_dict = pdfioObjGetDict(page_obj)) == NULL)
    return;

  if ((resources_dict = pdfioDictGetDict(page_dict, "Resources")) == NULL)
    return;

  if ((font_dict = pdfioDictGetDict(resources_dict, "Font")) == NULL)
  {
    // Font resources not a dictionary, see if it is an object...
    if ((font_obj = pdfioDictGetObj(resources_dict, "Font")) != NULL)
      font_dict = pdfioObjGetDict(font_obj);

    if (!font_dict)
      return;
  }

  if ((font_obj = pdfioDictGetObj(font_dict, name)) == NULL)
    return;

  if ((encoding_obj = pdfioDictGetObj(pdfioObjGetDict(font_obj), "Encoding")) == NULL)
    return;

  if ((encoding_dict = pdfioObjGetDict(encoding_obj)) == NULL)
    return;

  // OK, have the encoding object, build the encoding using it...
  base_encoding = pdfioDictGetName(encoding_dict, "BaseEncoding");
  differences   = pdfioDictGetArray(encoding_dict, "Differences");

  if (base_encoding && !strcmp(base_encoding, "MacRomanEncoding"))
  {
    // Map upper 128
    memcpy(encoding + 128, mac_roman, sizeof(mac_roman));
  }

  if (differences)
  {
    // Apply differences
    size_t	count = pdfioArrayGetSize(differences);
					// Number of differences
    const char	*name;			// Character name
    size_t	idx = 0;		// Index in encoding array

    for (i = 0; i < count; i ++)
    {
      switch (pdfioArrayGetType(differences, i))
      {
        case PDFIO_VALTYPE_NUMBER :
            // Get the index of the next character...
            idx = (size_t)pdfioArrayGetNumber(differences, i);
            break;

        case PDFIO_VALTYPE_NAME :
            // Lookup name and apply to encoding...
            if (idx < 0 || idx > 255)
              break;

            name = pdfioArrayGetName(differences, i);
            for (j = 0; j < (sizeof(unicode_map) / sizeof(unicode_map[0])); j ++)
            {
              if (!strcmp(name, unicode_map[j].name))
              {
                encoding[idx] = unicode_map[j].unicode;
                break;
              }
            }
	    idx ++;
            break;

        default :
            // Do nothing for other values
            break;
      }
    }
  }
}


//
// 'put_utf8()' - Output a single Unicode character as UTF-8.
//

static void
put_utf8(int ch)			// I - Character
{
  if (ch < 0x80)
  {
    // US ASCII
    putchar(ch);
  }
  else if (ch < 0x1000)
  {
    // 2-byte UTF-8
    putchar(0xc0 | (ch >> 6));
    putchar(0x80 | (ch & 0x3f));
  }
  else
  {
    // 3-byte UTF-8
    putchar(0xe0 | (ch >> 12));
    putchar(0x80 | ((ch >> 6) & 0x3f));
    putchar(0x80 | (ch & 0x3f));
  }
}


//
// 'puts_utf16()' - Output a hex-encoded UTF-16 string.
//

static void
puts_utf16(const char *s)		// I - Hex string
{
  size_t	length = strlen(s) / 4;	// Length of string
  int		ch;			// Character
  char		temp[5];		// Hex characters


  temp[4] = '\0';

  while (length > 0)
  {
    // Get the next Unicode character...
    temp[0] = *s++;
    temp[1] = *s++;
    temp[2] = *s++;
    temp[3] = *s++;
    length --;

    if ((ch = strtol(temp, NULL, 16)) < 0)
      break;

    if (ch >= 0xd800 && ch <= 0xdbff && length > 0)
    {
      // Multi-word UTF-16 char...
      int lch;			// Lower bits

      temp[0] = *s++;
      temp[1] = *s++;
      temp[2] = *s++;
      temp[3] = *s++;
      length --;

      if ((lch = strtol(temp, NULL, 16)) < 0 || lch < 0xdc00 || lch >= 0xdfff)
	break;

      ch = (((ch & 0x3ff) << 10) | (lch & 0x3ff)) + 0x10000;
    }

    put_utf8(ch);
  }
}
