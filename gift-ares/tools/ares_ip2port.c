#include <stdio.h>
#include <stdlib.h>

typedef unsigned int as_uint32;
typedef unsigned short as_uint16;
typedef unsigned char as_uint8;

#define FATAL_ERROR(x) { fprintf (stderr, "\nFATAL: %s\n", x); exit (1); }

/* ips may contain null bytes so we cannot rely on them for string
 * termination
 */
static unsigned short hash_lowered_token (unsigned char *str, int len)
{
	as_uint32 acc = 0;
	unsigned char c;
	int b = 0;

	/* this is a very poor hash function :( */
	for (; len > 0; len--, str++)
	{
		c = tolower (*str);
		acc ^= c << (b*8);
		b = (b + 1) & 3;
	}

	return (acc * 0x4f1bbcdc) >> 16;
}

static as_uint16 ip2port (as_uint32 ip)
{
	as_uint8 ip_str[4];
	as_uint8 tmp_str[4];
	as_uint16 ip_token;
	as_uint32 ebx;

	ip_str[0] = (ip >> 24) & 0xFF;
	ip_str[1] = (ip >> 16) & 0xFF;
	ip_str[2] = (ip >> 8)  & 0xFF;
	ip_str[3] = (ip)       & 0xFF;

	ip_token = hash_lowered_token (ip_str, 4);

	fprintf (stderr, "ip_token: 0x%04X\n", (int)ip_token);
	
	ebx = ((ip_str[0] * ip_str[0]) + ip_token) & 0xFFFF;
	ebx += ((ip_str[0] * ip_str[0]) + ip_token) & 0xFFFF;
	ebx += ((ip_str[0] * ip_str[0]) + ip_token) & 0xFFFF;

	fprintf (stderr, "ebx: 0x%08X\n", ebx);

	tmp_str[0] = ebx & 0xFF;;
	tmp_str[1] = (ebx >> 8) & 0xFF;
	tmp_str[2] = 0xBE;
	tmp_str[3] = 0x04;

	fprintf (stderr, "tmp_token: 0x%04X\n", (int)hash_lowered_token (tmp_str, 4));

	ebx += hash_lowered_token (tmp_str, 4);
	ebx += ip_token;
	ebx += 0x12;
	ebx += hash_lowered_token ("strano", 6);
	ebx -= ((ip_str[0] - 5) << 2) * 3;
	ebx += hash_lowered_token ("robboso", 7);

	fprintf (stderr, "strano_token: 0x%04X\n", (int)hash_lowered_token ("strano", 6));
	fprintf (stderr, "robboso_token: 0x%04X\n", (int)hash_lowered_token ("robboso", 7));

	fprintf (stderr, "ebx: 0x%08X\n", ebx);

	if (ebx < 1024)
		ebx += 1024;

	if (ebx == 36278)
		ebx++;

	return ebx & 0xFFFF;
}


int main (int argc, char* argv[])
{
	as_uint32 ip;
	as_uint32 ipa, ipb, ipc, ipd; /* don't want to require socket lib here */
	as_uint16 port;


	if (argc != 3)
	{
		fprintf (stderr, "Usage: %s <format> <ip>\n", argv[0]);
		fprintf (stderr, "Format is one of:\n"
		                 "- 'signed' ip is a signed integer\n"
						 "- 'dotted' ip is dotted decimal\n");
		exit (1);
	}

	if (!strcmp (argv[1], "signed"))
	{
		ip = (as_uint32) atoi (argv[2]);
		ipa = (ip >> 24) & 0xFF;
		ipb = (ip >> 16) & 0xFF;
		ipc = (ip >> 8) & 0xFF;
		ipd = (ip) & 0xFF;
	}
	else if (!strcmp (argv[1], "dotted"))
	{
		sscanf (argv[2], "%u.%u.%u.%u", &ipa, &ipb, &ipc, &ipd);
		ipa &= 0xFF; ipb &= 0xFF; ipc &= 0xFF; ipd &= 0xFF;
		ip = (ipa << 24) | (ipb << 16) | (ipc << 8)  | ipd;
	}
	else
	{
		FATAL_ERROR ("Invalid ip format");
	}


	port = ip2port (ip);

	printf ("%u.%u.%u.%u:%d\n", ipa, ipb, ipc, ipd, port);

}


