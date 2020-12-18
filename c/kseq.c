#include <stdlib.h>
#include <string.h>
#include <unistr.h>
#include <unicase.h>
#include "kseq.h"
#include "prog_util.h"

int kseq_cmp(const KSeq *a, const KSeq *b)
{
	int res;
	if (u32_casecmp(a->s, a->len, b->s, b->len, iso639_lang, NULL, &res))
		return 0;
	return res;
}

