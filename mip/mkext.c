/*	$Id$	*/

/*
 * Generate defines for the needed hardops.
 */
#include "pass2.h"
#include <stdlib.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_C99_FORMAT
#define FMTdPTR "%td"
#else
#if defined(_WIN64) || defined(LP64)
#define FMTdPTR "%ld"
#else
#define FMTdPTR "%d"
#endif
#endif

int chkop[DSIZE];

void mktables(void);

#ifdef NEWSHAPES
#define DEFSH(a,A)      struct shape a = { REG, A, 0 }
/* default shapes */
DEFSH(shareg, CLASSA);
DEFSH(shbreg, CLASSB);
DEFSH(shcreg, CLASSC);
DEFSH(shdreg, CLASSD);
DEFSH(shereg, CLASSE);
DEFSH(shfreg, CLASSF);
DEFSH(shgreg, CLASSG);
struct shape shoreg = { OREG, 0, 0 };
struct shape shname = { NAME, 0, 0 };
struct shape shicon = { ICON, MIN_LONGLONG, MAX_LONGLONG };
struct shape shone = { ICON, 1, 1 };
struct shape shzero = { ICON, 0, 0 };
#endif

char *ftitle;
char *cname = "external.c";
char *hname = "external.h";
FILE *fc, *fh;

/*
 * masks for matching dope with shapes
 */
int mamask[] = {
        SIMPFLG,                /* OPSIMP */
        SIMPFLG|ASGFLG,         /* ASG OPSIMP */
        COMMFLG,        /* OPCOMM */
        COMMFLG|ASGFLG, /* ASG OPCOMM */
        MULFLG,         /* OPMUL */
        MULFLG|ASGFLG,  /* ASG OPMUL */
        DIVFLG,         /* OPDIV */
        DIVFLG|ASGFLG,  /* ASG OPDIV */
        UTYPE,          /* OPUNARY */
        TYFLG,          /* ASG OPUNARY is senseless */
        LTYPE,          /* OPLEAF */
        TYFLG,          /* ASG OPLEAF is senseless */
        0,              /* OPANY */
        ASGOPFLG|ASGFLG,        /* ASG OPANY */
        LOGFLG,         /* OPLOG */
        TYFLG,          /* ASG OPLOG is senseless */
        FLOFLG,         /* OPFLOAT */
        FLOFLG|ASGFLG,  /* ASG OPFLOAT */
        SHFFLG,         /* OPSHFT */
        SHFFLG|ASGFLG,  /* ASG OPSHIFT */
        SPFLG,          /* OPLTYPE */
        TYFLG,          /* ASG OPLTYPE is senseless */
        };


struct checks {
	int op, type;
	char *name;
} checks[] = {
	{ MUL, TLONGLONG, "SMULLL", },
	{ DIV, TLONGLONG, "SDIVLL", },
	{ MOD, TLONGLONG, "SMODLL", },
	{ PLUS, TLONGLONG, "SPLUSLL", },
	{ MINUS, TLONGLONG, "SMINUSLL", },
	{ MUL, TULONGLONG, "UMULLL", },
	{ DIV, TULONGLONG, "UDIVLL", },
	{ MOD, TULONGLONG, "UMODLL", },
	{ PLUS, TULONGLONG, "UPLUSLL", },
	{ MINUS, TULONGLONG, "UMINUSLL", },
	{ 0, 0, 0, },
};

int rstatus[] = { RSTATUS };
int roverlay[MAXREGS][MAXREGS] = { ROVERLAP };
int regclassmap[CLASSG][MAXREGS]; /* CLASSG is highest class */
int rtemp[CLASSG][MAXREGS], rperm[CLASSG][MAXREGS], rprem[CLASSG][MAXREGS], runk[CLASSG][MAXREGS];
int ntemp[CLASSG], nperm[CLASSG], nprem[CLASSG], nunk[CLASSG];
int total_temp = 0, total_perm = 0, total_prem = 0, total_unk = 0;


static void
compl(struct optab *q, char *str)
{
	int op = q->op;
	char *s;

	if (op < OPSIMP) {
		s = opst[op];
	} else
		switch (op) {
		default:	s = "Special op";	break;
		case OPSIMP:	s = "OPLSIMP";	break;
		case OPCOMM:	s = "OPCOMM";	break;
		case OPMUL:	s = "OPMUL";	break;
		case OPDIV:	s = "OPDIV";	break;
		case OPUNARY:	s = "OPUNARY";	break;
		case OPLEAF:	s = "OPLEAF";	break;
		case OPANY:	s = "OPANY";	break;
		case OPLOG:	s = "OPLOG";	break;
		case OPFLOAT:	s = "OPFLOAT";	break;
		case OPSHFT:	s = "OPSHFT";	break;
		case OPLTYPE:	s = "OPLTYPE";	break;
		}

	printf("table entry " FMTdPTR ", op %s: %s\n", q - table, s, str);
}

/*
 * Find reg class for top of node.
 */
char *
hasneed(char *w, int need)
{
	for (; *w; w += NEEDADD(*w)) {
		if (*w == need)
			return w;
	}
	return 0;
}

static int
getrcl(struct optab *q)
{
	static int c[] = { 0, INAREG, INBREG, INCREG, INDREG, INEREG,
		INFREG, INGREG };
	int r = q->rewrite & RESC1 ? 1 : q->rewrite & RESC2 ? 2 : 3;
	int i = 0;
	char *w;

	if ((w = q->needs) == NULL)
		return 0;
	while ((w = hasneed(w, cNREG))) {
		if ((i += w[2]) >= r)
			return c[(int)w[1]];
		w += 3;
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	struct optab *q;
	struct checks *ch;
	int i, j, areg, breg, creg, dreg, mx, ereg, freg, greg;
	char *bitary;
	int bitsz, rval, nelem;

	if (argc == 2) {
		i = atoi(argv[1]);
		printf("Entry %d:\n%s\n", i, table[i].cstring);
		return 0;
	}

	mkdope();

	for (q = table; q->op != FREE; q++) {
		if (q->op >= OPSIMP)
			continue;
		if ((q->ltype & TLONGLONG) &&
		    (q->rtype & TLONGLONG))
			chkop[q->op] |= TLONGLONG;
		if ((q->ltype & TULONGLONG) &&
		    (q->rtype & TULONGLONG))
			chkop[q->op] |= TULONGLONG;
	}
	if ((fc = fopen(cname, "w")) == NULL) {
		perror("open cfile");
		return(1);
	}
	if ((fh = fopen(hname, "w")) == NULL) {
		perror("open hfile");
		return(1);
	}
	fprintf(fh, "#ifndef _EXTERNAL_H_\n#define _EXTERNAL_H_\n");

	for (ch = checks; ch->op != 0; ch++) {
		if ((chkop[ch->op] & ch->type) == 0)
			fprintf(fh, "#define NEED_%s\n", ch->name);
	}

	fprintf(fc, "#include \"pass2.h\"\n");
	/* create fast-lookup tables */
	mktables();

	/* create efficient bitset sizes */
	if (sizeof(long) == 8) { /* 64-bit arch */
		bitary = "long";
		bitsz = 64;
	} else {
		bitary = "int";
		bitsz = sizeof(int) == 4 ? 32 : 16;
	}
	fprintf(fh, "#define NUMBITS %d\n", bitsz);
	fprintf(fh, "#define BIT2BYTE(bits) "
	     "((((bits)+NUMBITS-1)/NUMBITS)*(NUMBITS/8))\n");
	fprintf(fh, "#define BITSET(arr, bit) "
	     "(arr[bit/NUMBITS] |= ((%s)1 << (bit & (NUMBITS-1))))\n",
	     bitary);
	fprintf(fh, "#define BITCLEAR(arr, bit) "
	     "(arr[bit/NUMBITS] &= ~((%s)1 << (bit & (NUMBITS-1))))\n",
	     bitary);
	fprintf(fh, "#define TESTBIT(arr, bit) "
	     "(arr[bit/NUMBITS] & ((%s)1 << (bit & (NUMBITS-1))))\n",
	     bitary);
	fprintf(fh, "typedef %s bittype;\n", bitary);

	/* register class definitions, used by graph-coloring */
	/* TODO */
	 
	 /* have to do some bookkeeping to use temp regs first and prem regs last */
	for (j = 0; j < NUMCLASS; j++) {
		int cl = SAREG << j;
		if (j > 3)
			cl = SEREG << (j - 4);
		for (i = 0; i < MAXREGS; i++) {
			if (rstatus[i] & cl) {
#if 0
printf("reg %d rstatus 0x%x %s\n", i, rstatus[i],
	((rstatus[i] & PREMREG) == PREMREG) ? "prem" : 
	rstatus[i] & PERMREG ? "perm" : 
	rstatus[i] & TEMPREG ? "temp" : "unk");
#endif
				if ((rstatus[i] & PREMREG) == PREMREG)
					rprem[j][nprem[j]++] = i;
				else if (rstatus[i] & PERMREG)
					rperm[j][nperm[j]++] = i;
				else if (rstatus[i] & TEMPREG)	
					rtemp[j][ntemp[j]++] = i;
				else
					runk[j][nunk[j]++] = i;					
			}
		}
		total_temp += ntemp[j];
		total_perm += nperm[j];
		total_prem += nprem[j];
		total_unk += nunk[j];
	}
	
	
	/* Sanity-check the table */
	rval = 0;
	for (q = table; q->op != FREE; q++) {
		switch (q->op) {
#ifndef NEWSHAPES
		case ASSIGN:
#define	F(x) (q->visit & x && q->rewrite & (RLEFT|RRIGHT) && \
		    q->lshape & ~x && q->rshape & ~x)
			if (F(INAREG) || F(INBREG) || F(INCREG) || F(INDREG) ||
			    F(INEREG) || F(INFREG) || F(INGREG)) {
				compl(q, "may match without result register");
				rval++;
			}
#undef F
			/* FALLTHROUGH */
#endif
		case STASG:
			if ((q->visit & INREGS) && !(q->rewrite & RDEST)) {
				compl(q, "ASSIGN/STASG reclaim must be RDEST");
				rval++;
			}
			break;
		case STCLR:
			fprintf(fh, "#define HAS_STCLR\n");
			break;
		}
		/* check that reclaim is not the wrong class */
		if ((q->rewrite & (RESC1|RESC2|RESC3)) && 
		    hasneed(q->needs, cNREW) == 0) {
			if ((q->visit & getrcl(q)) == 0) {
				compl(q, "wrong RESCx class");
				rval++;
			}
		}
		if (q->rewrite & (RESC1|RESC2|RESC3) && q->visit & FOREFF)
			compl(q, "FOREFF may cause reclaim of wrong class");
	}

	/* print out list of scratched and permanent registers */
	fprintf(fh, "extern int tempregs[], permregs[];\n");
	fprintf(fc, "int tempregs[] = { ");
	for (j = 0; j < NUMCLASS; j++) {
		for (i = 0; i < ntemp[j]; i++)
			fprintf(fc, "%d, ", rtemp[j][i]);
	}
	fprintf(fc, "-1 };\n");
	fprintf(fh, "#define NTEMPREG %d\n", total_temp+1);
	fprintf(fh, "#define FREGS %d\n", total_temp);	/* XXX - to die */
	fprintf(fc, "int permregs[] = { ");
	for (j = 0; j < NUMCLASS; j++) {
		for (i = 0; i < nperm[j]; i++)
			fprintf(fc, "%d, ", rperm[j][i]);
		for (i = 0; i < nprem[j]; i++)
			fprintf(fc, "%d, ", rprem[j][i]);
	}
	fprintf(fc, "-1 };\n");
	fprintf(fc, "\n");
	fprintf(fh, "#define NPERMREG %d\n", total_perm+1);
	fprintf(fc, "bittype validregs[] = {\n");

if (bitsz == 64) {
	for (j = 0; j < MAXREGS; j += bitsz) {
		long cbit = 0;
		for (i = 0; i < bitsz; i++) {
			if (i+j == MAXREGS)
				break;
			if (rstatus[i+j] & INREGS)
				cbit |= ((long)1 << i);
		}
		fprintf(fc, "\t0x%lx,\n", cbit);
	}
} else {
	for (j = 0; j < MAXREGS; j += bitsz) {
		int cbit = 0;
		for (i = 0; i < bitsz; i++) {
			if (i+j == MAXREGS)
				break;
			if (rstatus[i+j] & INREGS)
				cbit |= (1 << i);
		}
		fprintf(fc, "\t0x%08x,\n", cbit);
	}
}

	fprintf(fc, "};\n");
	fprintf(fh, "extern bittype validregs[];\n");

	/*
	 * The register allocator uses bitmasks of registers for each class.
	 */
 
	areg = breg = creg = dreg = ereg = freg = greg = 0;
	int atempc = 0, apermc = 0, apremc = 0;
	int btempc = 0, bpermc = 0, bpremc = 0;
	int ctempc = 0, cpermc = 0, cpremc = 0;
	int dtempc = 0, dpermc = 0, dpremc = 0;
	int etempc = 0, epermc = 0, epremc = 0;
	int ftempc = 0, fpermc = 0, fpremc = 0;
	int gtempc = 0, gpermc = 0, gpremc = 0;
	for (i = 0; i < MAXREGS; i++) {
		for (j = 0; j < NUMCLASS; j++)
			regclassmap[j][i] = -1;
		if (rstatus[i] & SAREG) { 
			areg++;
			regclassmap[0][i] =  ((rstatus[i] & PREMREG) == PREMREG) ? ntemp[0] + nperm[0] + (apremc++) : 
											((rstatus[i] & PERMREG) ? ntemp[0] + (apermc++) : (atempc++));
		}
		if (rstatus[i] & SBREG) { 
			breg++;
			regclassmap[1][i] =  ((rstatus[i] & PREMREG) == PREMREG) ? ntemp[1] + nperm[1] + (bpremc++) : 
											((rstatus[i] & PERMREG) ? ntemp[1] + (bpermc++) : (btempc++));
		}

		if (rstatus[i] & SCREG) { 
			creg++;
			regclassmap[2][i] =  ((rstatus[i] & PREMREG) == PREMREG) ? ntemp[2] + nperm[2] + (cpremc++) : 
											((rstatus[i] & PERMREG) ? ntemp[2] + (cpermc++) : (ctempc++));
		}
		if (rstatus[i] & SDREG) { 
			dreg++;
			regclassmap[3][i] =  ((rstatus[i] & PREMREG) == PREMREG) ? ntemp[3] + nperm[3] + (dpremc++) : 
											((rstatus[i] & PERMREG) ? ntemp[4] + (dpermc++) : (dtempc++));
		}
		if (rstatus[i] & SEREG) { 
			ereg++;
			regclassmap[4][i] =  ((rstatus[i] & PREMREG) == PREMREG) ? ntemp[4] + nperm[4] + (epremc++) : 
											((rstatus[i] & PERMREG) ? ntemp[4] + (epermc++) : (etempc++));
		}
		if (rstatus[i] & SFREG) { 
			freg++;
			regclassmap[5][i] =  ((rstatus[i] & PREMREG) == PREMREG) ? ntemp[5] + nperm[5] + (fpremc++) : 
											((rstatus[i] & PERMREG) ? ntemp[5] + (fpermc++) : (ftempc++));
		}
		if (rstatus[i] & SGREG) { 
			greg++;
			regclassmap[6][i] =  ((rstatus[i] & PREMREG) == PREMREG) ? ntemp[6] + nperm[6] + (gpremc++) : 
											((rstatus[i] & PERMREG) ? ntemp[6] + (gpermc++) : (gtempc++));
		}

	}
	fprintf(fh, "#define AREGCNT %d\n", areg);
	fprintf(fh, "#define BREGCNT %d\n", breg);
	fprintf(fh, "#define CREGCNT %d\n", creg);
	fprintf(fh, "#define DREGCNT %d\n", dreg);
	fprintf(fh, "#define EREGCNT %d\n", ereg);
	fprintf(fh, "#define FREGCNT %d\n", freg);
	fprintf(fh, "#define GREGCNT %d\n", greg);
	if (areg > bitsz)
		printf("%d regs in class A (max %d)\n", areg, bitsz), rval++;
	if (breg > bitsz)
		printf("%d regs in class B (max %d)\n", breg, bitsz), rval++;
	if (creg > bitsz)
		printf("%d regs in class C (max %d)\n", creg, bitsz), rval++;
	if (dreg > bitsz)
		printf("%d regs in class D (max %d)\n", dreg, bitsz), rval++;
	if (ereg > bitsz)
		printf("%d regs in class E (max %d)\n", ereg, bitsz), rval++;
	if (freg > bitsz)
		printf("%d regs in class F (max %d)\n", freg, bitsz), rval++;
	if (greg > bitsz)
		printf("%d regs in class G (max %d)\n", greg, bitsz), rval++;

	fprintf(fc, "static int amap[MAXREGS][NUMCLASS] = {\n");
	for (i = 0; i < MAXREGS; i++) {
		int ba, bb, bc, bd, r, be, bf, bg;
		ba = bb = bc = bd = be = bf = bg = 0;
		if (rstatus[i] & SAREG) ba = (1 << regclassmap[0][i]);
		if (rstatus[i] & SBREG) bb = (1 << regclassmap[1][i]);
		if (rstatus[i] & SCREG) bc = (1 << regclassmap[2][i]);
		if (rstatus[i] & SDREG) bd = (1 << regclassmap[3][i]);
		if (rstatus[i] & SEREG) be = (1 << regclassmap[4][i]);
		if (rstatus[i] & SFREG) bf = (1 << regclassmap[5][i]);
		if (rstatus[i] & SGREG) bg = (1 << regclassmap[6][i]);
		for (j = 0; roverlay[i][j] >= 0; j++) {
			r = roverlay[i][j];
			if (rstatus[r] & SAREG)
				ba |= (1 << regclassmap[0][r]);
			if (rstatus[r] & SBREG)
				bb |= (1 << regclassmap[1][r]);
			if (rstatus[r] & SCREG)
				bc |= (1 << regclassmap[2][r]);
			if (rstatus[r] & SDREG)
				bd |= (1 << regclassmap[3][r]);
			if (rstatus[r] & SEREG)
				be |= (1 << regclassmap[4][r]);
			if (rstatus[r] & SFREG)
				bf |= (1 << regclassmap[5][r]);
			if (rstatus[r] & SGREG)
				bg |= (1 << regclassmap[6][r]);
		}
		fprintf(fc, "\t/* %d */{ 0x%x", i, ba);
		if (NUMCLASS > 1) fprintf(fc, ",0x%x", bb);
		if (NUMCLASS > 2) fprintf(fc, ",0x%x", bc);
		if (NUMCLASS > 3) fprintf(fc, ",0x%x", bd);
		if (NUMCLASS > 4) fprintf(fc, ",0x%x", be);
		if (NUMCLASS > 5) fprintf(fc, ",0x%x", bf);
		if (NUMCLASS > 6) fprintf(fc, ",0x%x", bg);
		fprintf(fc, " },\n");
	}
	fprintf(fc, "};\n");

	fprintf(fh, "int aliasmap(int class, int regnum);\n");
	fprintf(fc, "int\naliasmap(int class, int regnum)\n{\n");
	fprintf(fc, "	return amap[regnum][class-1];\n}\n");

	/* routines to convert back from color to regnum */
	mx = areg;
	if (breg > mx) mx = breg;
	if (creg > mx) mx = creg;
	if (dreg > mx) mx = dreg;
	if (ereg > mx) mx = ereg;
	if (freg > mx) mx = freg;
	if (greg > mx) mx = greg;
	if (mx > (int)(sizeof(int)*8)-1) {
		printf("too many regs in a class, use two classes instead\n");
#ifdef HAVE_C99_FORMAT
		printf("%d > %zu\n", mx, (sizeof(int)*8)-1);
#else
		printf("%d > %d\n", mx, (int)(sizeof(int)*8)-1);
#endif
		rval++;
	}
#ifdef PCC_DEBUG
printf("TEMPREG 0x%x PERMREG 0x%x PREMREG 0x%x\n", TEMPREG, PERMREG, PREMREG);
#endif
	fprintf(fc, "static int rmap[NUMCLASS][%d] = {\n", mx);
	for (j = 0; j < NUMCLASS; j++) {
		fprintf(fc, "\t{ ");
		for (i = 0; i < ntemp[j]; i++)
				fprintf(fc, "%d, ", rtemp[j][i]);
		for (i = 0; i < nperm[j]; i++)
				fprintf(fc, "%d, ", rperm[j][i]);
		for (i = 0; i < nprem[j]; i++)
					fprintf(fc, "%d, ", rprem[i][i]);
		for (i = 0; i <nunk[j]; i++)
					fprintf(fc, "%d, ", runk[j][i]);
			fprintf(fc, "},\n");
	}
	fprintf(fc, "};\n\n");

	fprintf(fh, "int color2reg(int color, int class);\n");
	fprintf(fc, "int\ncolor2reg(int color, int class)\n{\n");
	fprintf(fc, "	return rmap[class-1][color];\n}\n");

	/* used by register allocator */
	fprintf(fc, "int regK[] = { 0, %d, %d, %d, %d, %d, %d, %d };\n",
	    areg, breg, creg, dreg, ereg, freg, greg);
	fprintf(fc, "int clregs[] = { 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x };\n",
	    (1 << areg)-1, (1 << breg)-1, (1 << creg)-1,
	    (1 << dreg)-1, (1 << ereg)-1, (1 << freg)-1, (1 << greg)-1);

	fprintf(fc, "int\nclassmask(int class)\n{\n");
	fprintf(fc, "\treturn clregs[class-1];\n}\n");

	fprintf(fh, "int interferes(int reg1, int reg2);\n");
	nelem = (MAXREGS+bitsz-1)/bitsz;
	fprintf(fc, "static bittype ovlarr[MAXREGS][%d] = {\n", nelem);
	for (i = 0; i < MAXREGS; i++) {
		unsigned long el[10];
		memset(el, 0, sizeof(el));
		el[i/bitsz] = (unsigned long)1 << (i % bitsz);
		for (j = 0; roverlay[i][j] >= 0; j++) {
			int k = roverlay[i][j];
			el[k/bitsz] |= ((unsigned long)1 << (k % bitsz));
		}
		fprintf(fc, "{ ");
		for (j = 0; j < MAXREGS; j += bitsz)
			fprintf(fc, "0x%lx, ", el[j/bitsz]);
		fprintf(fc, " },\n");
	}
	fprintf(fc, "};\n");

	fprintf(fc, "int\ninterferes(int reg1, int reg2)\n{\n");
	fprintf(fc, "return (TESTBIT(ovlarr[reg1], reg2)) != 0;\n}\n");
	fclose(fc);
	fprintf(fh, "#endif /* _EXTERNAL_H_ */\n");
	fclose(fh);
	return rval;
}

#define	P(x)	fprintf x

void
mktables(void)
{
	struct optab *op;
	int mxalen = 0, curalen;
	int i;

#if 0
	P((fc, "#include \"pass2.h\"\n\n"));
#endif
	for (i = 0; i <= MAXOP; i++) {
		curalen = 0;
		P((fc, "static int op%d[] = { ", i));
		if (dope[i] != 0)
		for (op = table; op->op != FREE; op++) {
			if (op->op < OPSIMP) {
				if (op->op == i) {
					P((fc, FMTdPTR ", ", op - table));
					curalen++;
				}
			} else {
				int opmtemp;
				if ((opmtemp=mamask[op->op - OPSIMP])&SPFLG) {
					if (i==NAME || i==ICON || i==TEMP ||
					    i==OREG || i == REG || i == FCON) {
						P((fc, FMTdPTR ", ",
						    op - table));
						curalen++;
					}
				} else if ((dope[i]&(opmtemp|ASGFLG))==opmtemp){
					P((fc, FMTdPTR ", ", op - table));
					curalen++;
				}
			}
		}
		if (curalen > mxalen)
			mxalen = curalen;
		P((fc, "-1 };\n"));
	}
	P((fc, "\n"));

	P((fc, "int *qtable[] = { \n"));
	for (i = 0; i <= MAXOP; i++) {
		P((fc, "	op%d,\n", i));
	}
	P((fc, "};\n"));
	P((fh, "#define MAXOPLEN %d\n", mxalen+1));
}
