/* 
 * Copyright (c) 2001, Dr Martin Porter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of the <ORGANIZATION> nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */



#include <stdlib.h>
#include <string.h>
#include "../include/libstemmer.h"
#include "../runtime/api.h"
#include "modules.h"

struct sb_stemmer {
    struct SN_env * (*create)(void);
    void (*close)(struct SN_env *);
    int (*stem)(struct SN_env *);

    struct SN_env * env;
};

extern const char **
sb_stemmer_list(void)
{
    return algorithm_names;
}

static stemmer_encoding sb_getenc(const char * charenc)
{
    struct stemmer_encoding * encoding;
    if (charenc == NULL) return ENC_UTF_8;
    for (encoding = encodings; encoding->name != 0; encoding++) {
	if (strcmp(encoding->name, charenc) == 0) break;
    }
    if (encoding->name == NULL) return ENC_UNKNOWN;
    return encoding->enc;
}

extern struct sb_stemmer *
sb_stemmer_new(const char * algorithm, const char * charenc)
{
    stemmer_encoding enc;
    struct stemmer_modules * module;
    struct sb_stemmer * stemmer =
	    (struct sb_stemmer *) malloc(sizeof(struct sb_stemmer));
    if (stemmer == NULL) return NULL;
    enc = sb_getenc(charenc);
    if (enc == ENC_UNKNOWN) return NULL;

    for (module = modules; module->name != 0; module++) {
	if (strcmp(module->name, algorithm) == 0 && module->enc == enc) break;
    }
    if (module->name == NULL) return NULL;
    
    stemmer->create = module->create;
    stemmer->close = module->close;
    stemmer->stem = module->stem;

    stemmer->env = stemmer->create();
    if (stemmer->env == NULL)
    {
        sb_stemmer_delete(stemmer);
        return NULL;
    }

    return stemmer;
}

void
sb_stemmer_delete(struct sb_stemmer * stemmer)
{
    if (stemmer == 0) return;
    if (stemmer->close == 0) return;
    stemmer->close(stemmer->env);
    stemmer->close = 0;
    free(stemmer);
}

const sb_symbol *
sb_stemmer_stem(struct sb_stemmer * stemmer, const sb_symbol * word, int size)
{
    int ret;
    if (SN_set_current(stemmer->env, size, (const symbol *)(word)))
    {
        stemmer->env->l = 0;
        return NULL;
    }
    ret = stemmer->stem(stemmer->env);
    if (ret < 0) return NULL;
    stemmer->env->p[stemmer->env->l] = 0;
    return (const sb_symbol *)(stemmer->env->p);
}

int
sb_stemmer_length(struct sb_stemmer * stemmer)
{
    return stemmer->env->l;
}
