#include "all.h"

static void
fixarg(Ref *pr, int k, Fn *fn)
{
	char buf[32];
	Ref r0, r1, r2;
	int s, n;
	Con *c;

	r0 = *pr;
	switch (rtype(r0)) {
	case RCon:
		r1 = newtmp("isel", k, fn);
		if (KBASE(k) == 0) {
			emit(Ocopy, k, r1, r0, R);
		} else {
			c = &fn->con[r0.val];
			n = gasstash(&c->bits, KWIDE(k) ? 8 : 4);
			vgrow(&fn->con, ++fn->ncon);
			c = &fn->con[fn->ncon-1];
			sprintf(buf, "fp%d", n);
			*c = (Con){.type = CAddr, .local = 1};
			c->label = intern(buf);
			r2 = newtmp("isel", Kl, fn);
			emit(Oload, k, r1, r2, R);
			emit(Ocopy, Kl, r2, CON(c-fn->con), R);
		}
		*pr = r1;
		break;
	case RTmp:
		s = fn->tmp[r0.val].slot;
		if (s == -1)
			break;
		r1 = newtmp("isel", Kl, fn);
		emit(Oaddr, Kl, r1, SLOT(s), R);
		*pr = r1;
		break;
	}
}

static void
selcmp(Ins i, int k, int op, Fn *fn)
{
	Ref r;
	int sign, swap, neg;

	switch (op) {
	case Cieq:
		r = newtmp("isel", k, fn);
		emit(Oreqz, i.cls, i.to, r, R);
		emit(Oxor, k, r, i.arg[0], i.arg[1]);
		fixarg(&curi->arg[0], k, fn);
		fixarg(&curi->arg[1], k, fn);
		return;
	case Cine:
		r = newtmp("isel", k, fn);
		emit(Ornez, i.cls, i.to, r, R);
		emit(Oxor, k, r, i.arg[0], i.arg[1]);
		fixarg(&curi->arg[0], k, fn);
		fixarg(&curi->arg[1], k, fn);
		return;
	case Cisge: sign = 1, swap = 0, neg = 1; break;
	case Cisgt: sign = 1, swap = 1, neg = 0; break;
	case Cisle: sign = 1, swap = 1, neg = 1; break;
	case Cislt: sign = 1, swap = 0, neg = 0; break;
	case Ciuge: sign = 0, swap = 0, neg = 1; break;
	case Ciugt: sign = 0, swap = 1, neg = 0; break;
	case Ciule: sign = 0, swap = 1, neg = 1; break;
	case Ciult: sign = 0, swap = 0, neg = 0; break;
	case NCmpI + Cfeq:
	case NCmpI + Cfge:
	case NCmpI + Cfgt:
	case NCmpI + Cfle:
	case NCmpI + Cflt:
	case NCmpI + Cfo:
	case NCmpI + Cfuo:
		sign = 0, swap = 0, neg = 0;
		break;
	case NCmpI + Cfne:
		sign = 0, swap = 0, neg = 1;
		i.op = KWIDE(k) ? Oceqd : Oceqs;
		break;
	default: abort();  /* XXX: implement */
	}
	if (op < NCmpI)
		i.op = sign ? Ocsltl : Ocultl;
	if (swap) {
		r = i.arg[0];
		i.arg[0] = i.arg[1];
		i.arg[1] = r;
	}
	if (neg) {
		r = newtmp("isel", i.cls, fn);
		emit(Oxor, i.cls, i.to, r, getcon(1, fn));
		i.to = r;
	}
	emiti(i);
	fixarg(&curi->arg[0], k, fn);
	fixarg(&curi->arg[1], k, fn);
}

static void
sel(Ins i, Fn *fn)
{
	Ref *iarg;
	int ck, cc;

	switch (i.op) {
	case Onop:
		break;
	case Oextub:
		i.op = Oand;
		i.arg[1] = getcon(0xff, fn);
		goto Emit;
	case Ocall:
		emiti(i);  /* XXX: should this use the default label and fixarg ignore Ocall? */
		break;
	default:
	Emit:
		if (iscmp(i.op, &ck, &cc)) {
			selcmp(i, ck, cc, fn);
			break;
		}
		emiti(i);
		iarg = curi->arg; /* fixarg() can change curi */
		fixarg(&iarg[0], argcls(&i, 0), fn);
		fixarg(&iarg[1], argcls(&i, 1), fn);
	}
}

static void
seljmp(Blk *b, Fn *fn)
{
	Ref r;

	if (b->jmp.type != Jjnz)
		return;
	/* TODO: replace cmp+jnz with beq/bne/blt[u]/bge[u] */
}

void
rv64_isel(Fn *fn)
{
	Blk *b, **sb;
	Ins *i;
	Phi *p;
	uint n;
	int al;
	int64_t sz;

	/* assign slots to fast allocs */
	b = fn->start;
	/* specific to NAlign == 3 */ /* or change n=4 and sz /= 4 below */
	for (al=Oalloc, n=4; al<=Oalloc1; al++, n*=2)
		for (i=b->ins; i<&b->ins[b->nins]; i++)
			if (i->op == al) {
				if (rtype(i->arg[0]) != RCon)
					break;
				sz = fn->con[i->arg[0].val].bits.i;
				if (sz < 0 || sz >= INT_MAX-15)
					err("invalid alloc size %"PRId64, sz);
				sz = (sz + n-1) & -n;
				sz /= 4;
				fn->tmp[i->to.val].slot = fn->slot;
				fn->slot += sz;
				*i = (Ins){.op = Onop};
			}

	for (b=fn->start; b; b=b->link) {
		curi = &insb[NIns];
		for (sb=(Blk*[3]){b->s1, b->s2, 0}; *sb; sb++)
			for (p=(*sb)->phi; p; p=p->link) {
				for (n=0; p->blk[n] != b; n++)
					assert(n+1 < p->narg);
				fixarg(&p->arg[n], p->cls, fn);
			}
		seljmp(b, fn);
		for (i=&b->ins[b->nins]; i!=b->ins;)
			sel(*--i, fn);
		b->nins = &insb[NIns] - curi;
		idup(&b->ins, curi, b->nins);
	}

	if (debug['I']) {
		fprintf(stderr, "\n> After instruction selection:\n");
		printfn(fn, stderr);
	}
}
