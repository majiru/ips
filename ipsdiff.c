#include <u.h>
#include <libc.h>
#include <bio.h>

#define PUT2(p, u) (p)[0] = (u)>>8, (p)[1] = (u)
#define PUT3(p, u) (p)[0] = (u)>>16, (p)[1] = (u)>>8, (p)[2] = (u)

Biobuf *ips;
typedef struct Bin Bin;
struct Bin{
	int fd;
	long dot;
	long n;
	uchar data[0xFFFF];
};

static Bin base, modded;
static int dump = 0;

static long
slurp(Bin *b)
{
	b->n = readn(b->fd, b->data, sizeof b->data);
	if(b->n < 0)
		sysfatal("read: %r");
	return b->n;
}

static void
emit(u32int off, uchar *data, u16int size)
{
	uchar buf[3 + 2];
	uchar buf2[2 + 1];
	uchar v, *p, *e;

	if(size == 0)
		return;

	PUT3(buf, off);
	if(size > 3 && data[0] == data[1]){
		p = data + 2;
		e = data + size;
		v = data[0];
		while(p < e && *p == v)
			p++;
		PUT2(buf2, size);
		if(p == e)
			size = 0;
		buf2[2] = v;
	}

	PUT2(buf+3, size);
	if(dump)
		fprint(2, "%ud %ud\n", off, size);
	if(Bwrite(ips, buf, sizeof buf) != sizeof buf)
		sysfatal("Bwrite: %r");

	if(size == 0 && Bwrite(ips, buf2, sizeof buf2) != sizeof buf2)
		sysfatal("Bwrite: %r");
	if(size != 0 && Bwrite(ips, data, size) != size)
		sysfatal("Bwrite: %r");
}

static void
diff(Bin *a, Bin *b)
{
	uchar *ap, *bp;
	uchar *ac, *bc;
	uchar *ae, *be;
	int x;

	ap = a->data;
	bp = b->data;
	ae = a->data + a->n;
	be = b->data + b->n;

	if(a->n != b->n)
		sysfatal("desync: %ld %ld", a->n, a->n);

	for(; ap < ae && bp < be; ap = ac, bp = bc){
		if(*ap == *bp){
			ac = ap+1;
			bc = bp+1;
			continue;
		}
		x = 2;
		for(ac = ap, bc = bp; ac < ae && bc < be; ac++, bc++){
			if(*ac == *bc)
				x--;
			else
				x = 2;
			if(x == 0)
				break;
		}
		emit(a->dot + (ap - a->data), bp, bc - bp);
	}
	a->dot += a->n;
	b->dot += b->n;
}

void
usage(void)
{
	fprint(2, "usage: %s base modified >patch.ips", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	ARGBEGIN{
	case 'd':
		dump++;
		break;
	default:
		usage();
	}ARGEND;
	if(argc < 2)
		usage();

	ips = Bfdopen(1, OWRITE);
	if(ips == nil)
		sysfatal("Bfdopen: %r");
	Bwrite(ips, "PATCH", 5);

	base.fd = open(argv[0], OREAD);
	modded.fd = open(argv[1], OREAD);
	if(base.fd < 0 || modded.fd < 0)
		sysfatal("open: %r");

	while(slurp(&base) != 0 && slurp(&modded) != 0)
		diff(&base, &modded);

	Bwrite(ips, "EOF", 3);
	Bflush(ips);
	exits(nil);
}
