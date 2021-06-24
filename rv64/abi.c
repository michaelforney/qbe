#include "all.h"

typedef struct Class Class;
typedef struct Insl Insl;
typedef struct Params Params;

enum {
	Cstk = 1, /* pass on the stack */
	Cptr = 2, /* replaced by a pointer */
};

struct Class {
	char class;
	uint size;
	int reg[4];
	int cls[4];
};

struct Insl {
	Ins i;
	Insl *link;
};

struct Params {
	uint ngp;
	uint nfp;
	uint stk; /* stack offset for varargs */
};

static int gpreg[] = { A0,  A1,  A2,  A3,  A4,  A5,  A6,  A7};
static int fpreg[] = {FA0, FA1, FA2, FA3, FA4, FA5, FA6, FA7};

/* layout of call's second argument (RCall)
 *
 *  29   12    8    4  2  0
 *  |0.00|x|xxxx|xxxx|xx|xx|                  range
 *        |    |    |  |  ` gp regs returned (0..2)
 *        |    |    |  ` fp regs returned    (0..2)
 *        |    |    ` gp regs passed         (0..8)
 *        |     ` fp regs passed             (0..8)
 *        ` is x8 used                       (0..1)
 */

bits
rv64_retregs(Ref r, int p[2])
{
	bits b;
	int ngp, nfp;

	assert(rtype(r) == RCall);
	ngp = r.val & 3;
	nfp = (r.val >> 2) & 3;
	if (p) {
		p[0] = ngp;
		p[1] = nfp;
	}
	b = 0;
	while (ngp--)
		b |= BIT(A0+ngp);
	while (nfp--)
		b |= BIT(FA0+nfp);
	return b;
}

bits
rv64_argregs(Ref r, int p[2])
{
	bits b;
	int ngp, nfp;

	assert(rtype(r) == RCall);
	ngp = (r.val >> 4) & 15;
	nfp = (r.val >> 8) & 15;
	b = 0;
	if (p) {
		p[0] = ngp;
		p[1] = nfp;
	}
	b = 0;
	while (ngp--)
		b |= BIT(A0+ngp);
	while (nfp--)
		b |= BIT(FA0+nfp);
	return b;
}

static void
selret(Blk *b, Fn *fn)
{
	int j, k, cty;
	Ref r;

	j = b->jmp.type;

	if (!isret(j) || j == Jret0)
		return;

	r = b->jmp.arg;
	b->jmp.type = Jret0;

	if (j == Jretc) {
		die("unimplemented");
	} else {
		k = j - Jretw;
		if (KBASE(k) == 0) {
			emit(Ocopy, k, TMP(A0), r, R);
			cty = 1;
		} else {
			emit(Ocopy, k, TMP(FA0), r, R);
			cty = 1 << 2;
		}
	}

	b->jmp.arg = CALL(cty);
}

static int
argsclass(Ins *i0, Ins *i1, Class *carg)
{
	int ngp, nfp, *gp, *fp;
	Class *c;
	Ins *i;

	gp = gpreg;
	fp = fpreg;
	ngp = 8;
	nfp = 8;
	for (i=i0, c=carg; i<i1; i++, c++) {
		switch (i->op) {
		case Opar:
		case Oarg:
			c->cls[0] = i->cls;
			c->size = 8;
			if (KBASE(i->cls) == 0 && ngp > 0) {
				ngp--;
				c->reg[0] = *gp++;
			} else if (KBASE(i->cls) == 1 && nfp > 0) {
				nfp--;
				c->reg[0] = *fp++;
			} else {
				c->class |= Cstk;
			}
			break;
		case Oparc:
		case Oargc:
			abort();
		}
	}
	return (gp-gpreg) << 4 | (fp-fpreg) << 8;
}

static void
selcall(Fn *fn, Ins *i0, Ins *i1, Insl **ilp)
{
	Ins *i;
	Class *ca, *c;
	int cty;
	uint64_t stk;

	ca = alloc((i1-i0) * sizeof ca[0]);
	cty = argsclass(i0, i1, ca);

	stk = 0;
	for (i=i0, c=ca; i<i1; i++, c++) {
		if (c->class & Cptr) {
			abort();  /* XXX: implement */
		}
		if (c->class & Cstk) {
			abort();  /* XXX: implement */
		}
	}
	//if (stk)
	//	emit(Oadd, Kl, TMP(SP), TMP(SP)

	if (!req(i1->arg[1], R)) {
		abort();  /* XXX: implement */
	} else if (KBASE(i1->cls) == 0) {
		emit(Ocopy, i1->cls, i1->to, TMP(A0), R);
		cty |= 1;
	} else {
		emit(Ocopy, i1->cls, i1->to, TMP(FA0), R);
		cty |= 1 << 2;
	}

	emit(Ocall, 0, R, i1->arg[0], CALL(cty));

	/* move arguments into registers */
	for (i=i0, c=ca; i<i1; i++, c++) {
		if (c->class & Cstk)
			continue;
		if (i->op == Oargc)
			abort();  /* XXX: implement */
		else
			emit(Ocopy, *c->cls, TMP(*c->reg), i->arg[0], R);
	}
}

static Params
selpar(Fn *fn, Ins *i0, Ins *i1)
{
	Class *ca, *c;
	Ins *i;
	int s, cty;
	Ref r;

	ca = alloc((i1-i0) * sizeof ca[0]);
	curi = &insb[NIns];

	cty = argsclass(i0, i1, ca);
	fn->reg = rv64_argregs(CALL(cty), 0);

	for (i=i0, c=ca, s=2; i<i1; i++, c++) {
		if (c->class & Cstk) {
			r = newtmp("abi", Kl, fn);
			emit(Oload, c->cls[0], i->to, r, R);
			emit(Oaddr, Kl, r, SLOT(-s), R);
			s++;
		} else {
			emit(Ocopy, c->cls[0], i->to, TMP(c->reg[0]), R);
		}
	}

	return (Params){
		.stk = s,
		.ngp = (cty >> 4) & 15,
		.nfp = (cty >> 8) & 15,
	};
}

static void
selvaarg(Fn *fn, Blk *b, Ins *i)
{
	Ref loc, newloc;

	loc = newtmp("abi", Kl, fn);
	newloc = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, R, newloc, i->arg[0]);
	emit(Oadd, Kl, newloc, loc, getcon(8, fn));
	emit(Oload, i->cls, i->to, loc, R);
	emit(Oload, Kl, loc, i->arg[0], R);
}

static void
selvastart(Fn *fn, Params p, Ref ap)
{
	Ref rsave;
	int s;

	rsave = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, R, rsave, ap);
	s = p.stk > 2 ? p.stk : 2 + p.ngp;
	emit(Oaddr, Kl, rsave, SLOT(-s), R);
}

void
rv64_abi(Fn *fn)
{
	Blk *b;
	Ins *i, *i0, *ip;
	Insl *il;
	int n;
	Params p;

	for (b=fn->start; b; b=b->link)
		b->visit = 0;

	/* lower parameters */
	for (b=fn->start, i=b->ins; i<&b->ins[b->nins]; i++)
		if (!ispar(i->op))
			break;
	p = selpar(fn, b->ins, i);
	n = b->nins - (i - b->ins) + (&insb[NIns] - curi);
	i0 = alloc(n * sizeof(Ins));
	ip = icpy(ip = i0, curi, &insb[NIns] - curi);
	ip = icpy(ip, i, &b->ins[b->nins] - i);
	b->nins = n;
	b->ins = i0;

	/* lower calls, returns, and vararg instructions */
	il = 0;
	b = fn->start;
	do {
		if (!(b = b->link))
			b = fn->start; /* do it last */
		if (b->visit)
			continue;
		curi = &insb[NIns];
		selret(b, fn);
		for (i=&b->ins[b->nins]; i!=b->ins;)
			switch ((--i)->op) {
			default:
				emiti(*i);
				break;
			case Ocall:
			case Ovacall:
				for (i0=i; i0>b->ins; i0--)
					if (!isarg((i0-1)->op))
						break;
				selcall(fn, i0, i, &il);
				i = i0;
				break;
			case Ovastart:
				selvastart(fn, p, i->arg[0]);
				break;
			case Ovaarg:
				selvaarg(fn, b, i);
				break;
			case Oarg:
			case Oargc:
				die("unreachable");
			}
		if (b == fn->start)
			for (; il; il=il->link)
				emiti(il->i);
		b->nins = &insb[NIns] - curi;
		idup(&b->ins, curi, b->nins);
	} while (b != fn->start);

	if (debug['A']) {
		fprintf(stderr, "\n> After ABI lowering:\n");
		printfn(fn, stderr);
	}
}
