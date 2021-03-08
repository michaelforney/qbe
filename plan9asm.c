#include "all.h"

static void
emitdat(Dat *d, FILE *f)
{
	static char *name;
	static int64_t off;
	char *p;
	size_t n;

	switch (d->type) {
	case DStart:
		break;
	case DEnd:
		fprintf(f, "GLOBL\t%s%s(SB), $%"PRId64"\n",
			name, d->export ? "" : "<>", off);
		break;
	case DName:
		name = d->u.str;
		break;
	case DZ:
		off += d->u.num;
		break;
	default:
		if (d->isstr) {
			if (d->type != DB)
				err("strings only supported for 'b' currently");
			n = strlen(d->u.str);
			/* XXX: maximum length 8 */
			fprintf(f, "DATA\t%s%s+%"PRId64"(SB)/%zu, $%s\n",
				name, d->export ? "" : "<>", off, n, d->u.str);
			off += n;
		}
		/*
		else if (d->isref) {
			p = d->u.ref.nam[0] == '"' ? "" : gassym;
			fprintf(f, "%s %s%s%+"PRId64"\n",
				dtoa[d->type], p, d->u.ref.nam,
				d->u.ref.off);
		}
		else {
			fprintf(f, "%s %"PRId64"\n",
				dtoa[d->type], d->u.num);
		}
		*/
		break;
	}
}

typedef struct Asmbits Asmbits;

struct Asmbits {
	char bits[16];
	int size;
	Asmbits *link;
};

static Asmbits *stash;

static int
stashbits(void *bits, int size)
{
	Asmbits **pb, *b;
	int i;

	assert(size == 4 || size == 8 || size == 16);
	for (pb=&stash, i=0; (b=*pb); pb=&b->link, i++)
		if (size <= b->size)
		if (memcmp(bits, b->bits, size) == 0)
			return i;
	b = emalloc(sizeof *b);
	memcpy(b->bits, bits, size);
	b->size = size;
	b->link = 0;
	*pb = b;
	return i;
}

static void
emitfin(FILE *f)
{
	Asmbits *b;
	char *p;
	int sz, i;
	double d;

	if (!stash)
		return;
	fprintf(f, "/* floating point constants */\n.data\n");
	for (sz=16; sz>=4; sz/=2)
		for (b=stash, i=0; b; b=b->link, i++) {
			if (b->size == sz) {
				fprintf(f,
					".balign %d\n"
					"%sfp%d:",
					sz, asm.loc, i
				);
				for (p=b->bits; p<&b->bits[sz]; p+=4)
					fprintf(f, "\n\t.int %"PRId32,
						*(int32_t *)p);
				if (sz <= 8) {
					if (sz == 4)
						d = *(float *)b->bits;
					else
						d = *(double *)b->bits;
					fprintf(f, " /* %f */\n", d);
				} else
					fprintf(f, "\n");
			}
		}
	while ((b=stash)) {
		stash = b->link;
		free(b);
	}
}

Asm asm_plan9 = {
	.type = Asmplan9,
	.stash = stashbits,
	.emitdat = emitdat,
	.emitfin = emitfin,
	.loc = "",
	.sym = "",
};
