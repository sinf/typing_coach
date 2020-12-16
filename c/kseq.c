#include <stdlib.h>
#include <string.h>
#include <unistr.h>
#include <unicase.h>
#include "kseq.h"
#include "prog_util.h"

int kseq_to_wchar(KSeq *seq, wchar_t buf[], int buflen)
{
	size_t csl=0;
	uint8_t *cs = u32_to_u8(seq->s, seq->len, NULL, &csl);
	int count = mbstowcs(buf, (const char*) cs, buflen-1);
	buf[buflen-1] = L'\0';
	free(cs);
	return count;
}

int kseq_equal(const KSeq *a, const KSeq *b)
{
	int res;
	if (u32_casecmp(a->s, a->len, b->s, b->len, iso639_lang, NULL, &res))
		return 0;
	return !res;
}

