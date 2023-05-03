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
	long n, off;
	uchar data[0xFFFF];
};

static Bin base, modded;
static int dump = 0;

static long
slurp(Bin *b)
{
	long n;

	n = readn(b->fd, b->data + b->off, sizeof b->data - b->off);
	if(n < 0)
		sysfatal("read: %r");
	b->n = b->off + n;
	return n;
}

static void
emit(u32int off, uchar *data, u16int size)
{
	uchar buf[3 + 2];

	if(size == 0)
		return;
	PUT3(buf, off);
	PUT2(buf+3, size);
	if(dump)
		fprint(2, "%ud %ud\n", off, size);
	if(Bwrite(ips, buf, sizeof buf) != sizeof buf)
		sysfatal("Bwrite: %r");
	if(Bwrite(ips, data, size) != size)
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

	while(ap < ae && bp < be){
		if(*ap == *bp){
			ap++;
			bp++;
			continue;
		}
		// FIXME: better way of expanding diff context
		x = 32;
		for(ac = ap, bc = bp; ac < ae && bc < be; ac++, bc++){
			if(--x <= 0 && *ac == *bc)
				break;
		}
		emit(a->dot + (ap - a->data), bp, bc - bp);
		ap = ac;
		bp = bc;
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
