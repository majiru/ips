#include <u.h>
#include <libc.h>
#include <bio.h>

#define GET2(p) (u16int)(p)[1] | (u16int)(p)[0]<<8
#define GET3(p) (u16int)(p)[2] | (u16int)(p)[1]<<8 | (u16int)(p[0])<<16

enum{
	Hdrsz = 3 + 2,
	RLEsz = 2 + 1,
};

static uchar section[Hdrsz + 0xFFFF + RLEsz];
static Biobuf *ips;
static int dump = 0;

static int
readsect(u32int *off, u16int *sz)
{
	u16int count;
	uchar val;

	if(Bread(ips, section, 3) != 3)
		sysfatal("short read of off: %r");
	if(memcmp(section, "EOF", 3) == 0)
		return 0;
	if(Bread(ips, section + 3, 2) != 2)
		sysfatal("short read of size: %r");
	*off = GET3(section);

	*sz = GET2(section + 3);
	if(*sz == 0){
		if(Bread(ips, section + Hdrsz + 0xFFFF, RLEsz) != RLEsz)
			sysfatal("short read of RLE: %r");
		count = GET2(section + Hdrsz + 0xFFFF);
		val = section[Hdrsz + 0xFFFF + 2];
		memset(section + Hdrsz, val, count);
		*sz = count;
	} else if(Bread(ips, section + Hdrsz, *sz) != *sz)
		sysfatal("short read of data: %r");
	if(dump)
		fprint(2, "%ud %ud\n", *off, *sz);
	return 1;
}

void
usage(void)
{
	fprint(2, "usage: %s patch.ips <file.orig >file.new\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	ulong dot, x;
	long n;
	uchar buf[8192];
	u32int off;
	u16int sz;

	ARGBEGIN{
	case 'd':
		dump++;
		break;
	default:
		usage();
	}ARGEND;
	if(argc < 1)
		usage();

	ips = Bopen(argv[0], OREAD);
	if(ips == nil)
		sysfatal("open: %r");

	if(Bread(ips, buf, 5) != 5)
		sysfatal("short read on magic: %r");
	if(memcmp(buf, "PATCH", 5) != 0)
		sysfatal("bad magic");

	if(readsect(&off, &sz) == 0)
		sysfatal("not a single section");

	for(dot = 0; (n = read(0, buf, sizeof buf)) > 0; dot += n){
		while(off < dot+n){
			if(off < dot)
				sysfatal("skipped region");
			if(off + sz > dot + n){
				x = (dot + n) - off;
				memcpy(buf + (off - dot), section + Hdrsz, x);
				memmove(section + Hdrsz, section + Hdrsz + x, sz - x);
				sz -= x;
				off += x;
				break;
			}
			memcpy(buf + (off - dot), section + Hdrsz, sz);
			if(readsect(&off, &sz) == 0)
				break;
		}
		write(1, buf, n);
	}
	exits(nil);
}
