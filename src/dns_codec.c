
/**
  *
  * Resolv::DNS::Codec class
  *
  *
  **/

#if __linux__ == 0
#error "archtecture other than linux is not supported"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mruby.h"

#include "dns_codec.h"
#include "dns_types.h"

/**
 * Put
 **/

mrb_dns_put_state *mrb_dns_codec_put_open(mrb_state *mrb) {
    mrb_dns_put_state *putter = (mrb_dns_put_state *)mrb_malloc(mrb, sizeof(mrb_dns_put_state));
    putter->buff              = NULL;
    putter->size              = 0;
    return putter;
}

int mrb_dns_codec_put_close(mrb_state *mrb, mrb_dns_put_state *putter) {
    if (!putter)
        return 0;

    if (putter->buff)
        mrb_free(mrb, putter->buff);

    if (putter)
        mrb_free(mrb, putter);

    return 0;
}

// put a octtet int bytes
int mrb_dns_codec_put_uint8(mrb_state *mrb, mrb_dns_put_state *putter, uint8_t w) {
    const int size = putter->size + 1;
    uint8_t *b     = (uint8_t *)mrb_malloc(mrb, size);

    if (putter->size > 0)
        memcpy(b, putter->buff, putter->size);
    b[size - 1] = w;

    if (putter->buff)
        mrb_free(mrb, putter->buff);

    putter->buff = b;
    putter->size = size;
    return 0;
}

// put 2 octtets value as big endian into bytes
int mrb_dns_codec_put_uint16be(mrb_state *mrb, mrb_dns_put_state *putter, uint16_t w) {
    uint8_t w1 = 0, w2 = 0;
    w1 = w & 0x00ff;
    w2 = (w & 0xff00) >> 8;

    if (mrb_dns_codec_put_uint8(mrb, putter, w2)) {
        return -1;
    }
    if (mrb_dns_codec_put_uint8(mrb, putter, w1)) {
        return -1;
    }

    return 0;
}

/**
 * put bytes
 */
int mrb_dns_codec_put_str(mrb_state *mrb, mrb_dns_put_state *putter, char *buff, size_t len) {
    const int size = putter->size + len;
    char *b        = (char *)mrb_malloc(mrb, size);

    if (putter->size > 0)
        memcpy(b, putter->buff, putter->size);

    strncpy(b + putter->size, buff, len);
    if (putter->buff)
        mrb_free(mrb, putter->buff);

    putter->buff = (uint8_t *)b;
    putter->size = size;
    return 0;
}

/**
 * Put DNS Packet
 **/


int mrb_dns_codec_put_name(mrb_state *mrb, mrb_dns_put_state *putter, mrb_dns_name_t *name) {
    char *tok = NULL;
    int len   = 0;

    if (!name) {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "mrb_dns_codec_put_name: null pointer");
        return -1;
    }
    if (!name->name) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "QUESTION(NAME) failure");
        return -1;
    }
    tok = strtok(name->name, ".");
    if (tok) {
        // TODO: remove strtok for thread-aware
        for (; tok != NULL; tok = strtok(NULL, ".")) {
            len = strlen(tok);
            if (len == 0)
                break;

            if (mrb_dns_codec_put_uint8(mrb, putter, len)) {
                mrb_raise(mrb, E_RUNTIME_ERROR, "QUESTION(NAME(DIGIT))");
                return -1;
            }
            if (mrb_dns_codec_put_str(mrb, putter, tok, len)) {
                mrb_raise(mrb, E_RUNTIME_ERROR, "QUESTION(NAME(STR))");
                return -1;
            }
        }
    }
    if (mrb_dns_codec_put_uint8(mrb, putter, 0)) {
        return -1;
    }
    return 0;
}

int mrb_dns_codec_put_header(mrb_state *mrb, mrb_dns_put_state *putter, mrb_dns_header_t *hdr) {
    uint8_t w1 = 0, w2 = 0;
    if (mrb_dns_codec_put_uint16be(mrb, putter, hdr->id)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "header(ID) put failure");
        return -1;
    };
    // QR(1) | OPCODE(4) | AA(1)  | TC(1) | RD(1)
    w1 = (uint8_t)((hdr->qr << 7) | (hdr->opcode << 3) | (hdr->aa << 2) | (hdr->tc << 1) | hdr->rd);
    if (mrb_dns_codec_put_uint8(mrb, putter, w1)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "header(QR|OPCOD|AA|TC|RD) put failure");
        return -1;
    };

    // RA(1) | Z(3) | RCODE(4)
    w2 = (uint8_t)((hdr->ra << 7) | (hdr->z << 4) | (hdr->rcode));
    if (mrb_dns_codec_put_uint8(mrb, putter, w2)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "header(RA|Z|RCODE) put failure");
        return -1;
    }
    if (mrb_dns_codec_put_uint16be(mrb, putter, hdr->qdcount)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "header(QDCOUNT) put failure");
        return -1;
    }
    if (mrb_dns_codec_put_uint16be(mrb, putter, hdr->ancount)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "header(ANCOUNT) put failure");
        return -1;
    }
    if (mrb_dns_codec_put_uint16be(mrb, putter, hdr->nscount)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "header(NSCOUNT) put failure");
        return -1;
    }
    if (mrb_dns_codec_put_uint16be(mrb, putter, hdr->arcount)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "header(ARCOUNT) put failure");
        return -1;
    }
    return 0;
}

int mrb_dns_codec_put_question(mrb_state *mrb, mrb_dns_put_state *putter, mrb_dns_question_t *q) {
    if (!q) {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "mrb_dns_codec_put_question: null pointer");
        return -1;
    }
    if (mrb_dns_codec_put_name(mrb, putter, q->qname)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "QUESTION(NAME) put failure");
        return -1;
    }
    if (mrb_dns_codec_put_uint16be(mrb, putter, q->qtype)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "inviald type in question section");
        return -1;
    }
    if (mrb_dns_codec_put_uint16be(mrb, putter, q->qklass)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "inviald class in question section");
        return -1;
    }
    return 0;
}

int mrb_dns_codec_put_rdata(mrb_state *mrb, mrb_dns_put_state *putter, mrb_dns_rdata_t *rdata) {
    if (!rdata) {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "mrb_dns_codec_put_rdata: empty RData arguments");
        return -1;
    }
    if (mrb_dns_codec_put_name(mrb, putter, rdata->name))
        return -1;
    if (mrb_dns_codec_put_uint16be(mrb, putter, rdata->typ))
        return -1;
    if (mrb_dns_codec_put_uint16be(mrb, putter, rdata->klass))
        return -1;
    if (mrb_dns_codec_put_uint16be(mrb, putter, rdata->ttl))
        return -1;
    if (mrb_dns_codec_put_uint16be(mrb, putter, rdata->rlength))
        return -1;
    if (mrb_dns_codec_put_str(mrb, putter, (char *)rdata->rdata, rdata->rlength))
        return -1;
    return 0;
}

int mrb_dns_codec_put(mrb_state *mrb, mrb_dns_put_state *putter, mrb_dns_pkt_t *pkt) {
    if (!putter) {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "mrb_dns_codec_put: putter is null pointer");
        return -1;
    }
    if (!pkt) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "mrb_dns_codec_put: pkt is null pointer");
        return -1;
    }
    if (!pkt->header) {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "mrb_dns_codec_put: pkt->header is null pointer");
        return -1;
    }
    if (mrb_dns_codec_put_header(mrb, putter, pkt->header)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "failure: put packet.header ");
        return -1;
    }

    for (int i = 0; i < pkt->header->qdcount; i++) {
        if (mrb_dns_codec_put_question(mrb, putter, pkt->questions[i])) {
            char buff[1024];
            sprintf(buff, "failure: put packet.question[%d] record", i);
            mrb_raise(mrb, E_RUNTIME_ERROR, buff);
            return -1;
        }
    }
    for (int i = 0; i < pkt->header->ancount; i++) {
        if (mrb_dns_codec_put_rdata(mrb, putter, pkt->answers[i])) {
            char buff[1024];
            sprintf(buff, "failure: put packet.answer[%d] record", i);
            mrb_raise(mrb, E_RUNTIME_ERROR, buff);
            return -1;
        }
    }
    for (int i = 0; i < pkt->header->nscount; i++) {
        if (mrb_dns_codec_put_rdata(mrb, putter, pkt->authorities[i])) {
            char buff[1024];
            sprintf(buff, "failure: put packet.authorty[%d] record", i);
            mrb_raise(mrb, E_RUNTIME_ERROR, buff);
            return -1;
        }
    }
    for (int i = 0; i < pkt->header->arcount; i++) {
        if (mrb_dns_codec_put_rdata(mrb, putter, pkt->additionals[i])) {
            char buff[1024];
            sprintf(buff, "failure: put packet.addtional[%d] record", i);
            mrb_raise(mrb, E_RUNTIME_ERROR, buff);
            return -1;
        }
    }
    if (!putter->buff) {
        mrb_raise(mrb, E_NOTIMP_ERROR, "falure: empty put result");
        return -1;
    }

    return 0;
}

uint8_t *mrb_dns_codec_put_result(mrb_state *mrb, mrb_dns_put_state *putter) {
    return putter->buff;
}

/**
 *  Get
 **/

mrb_dns_get_state *mrb_dns_codec_get_open(mrb_state *mrb, uint8_t *buff, size_t len) {
    mrb_dns_get_state *state = (mrb_dns_get_state *)mrb_malloc(mrb, sizeof(mrb_dns_get_state));
    uint8_t *b               = (uint8_t *)mrb_malloc(mrb, len);
    memcpy(b, buff, len);

    state->buff = b;
    state->pos  = 0;
    state->end  = len;
    return state;
}

int mrb_dns_codec_get_close(mrb_state *mrb, mrb_dns_get_state *getter) {
    if (getter->buff)
        mrb_free(mrb, getter->buff);
    if (getter)
        mrb_free(mrb, getter);
    getter = NULL;
    return 0;
}

int mrb_dns_codec_get_uint8(mrb_state *mrb, mrb_dns_get_state *getter, uint8_t *w) {
    uint64_t pos = 0;
    mrb_assert(w != NULL);

    pos = getter->pos;
    if (pos + 1 > getter->end)
        return -1;

    *w = getter->buff[pos];
    getter->pos++;

    return 0;
}

int mrb_dns_codec_get_uint16be(mrb_state *mrb, mrb_dns_get_state *getter, uint16_t *w) {
    uint16_t ret = 0;
    uint8_t w1 = 0, w2 = 0;
    mrb_assert(w != NULL);

    if (mrb_dns_codec_get_uint8(mrb, getter, &w1)) {
        return -1;
    }
    if (mrb_dns_codec_get_uint8(mrb, getter, &w2)) {
        return -1;
    }
    ret = w1 << 8 | w2;
    *w = ret;
    return 0;
}

int mrb_dns_codec_get_str(mrb_state *mrb, mrb_dns_get_state *getter, uint64_t size, char *dist) {
    char *d = NULL;
    if (dist != NULL) {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "mrb_dns_codec_get_str: dist");
        return -1;
    }
    d = (char *)malloc(size + 1);

    if (getter->pos + size > getter->end) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "mrb_dns_codec_get_str: reach end fo buffer");
        return -1;
    }
    memcpy(d, getter->buff + getter->pos, size);
    getter->pos += size;
    dist = d;

    return 0;
}

/**
 * Put DNS Packet
 **/


int mrb_dns_codec_get_header(mrb_state *mrb, mrb_dns_get_state *getter, mrb_dns_header_t *ret) {
    uint8_t w1 = 0, w2 = 0;
    mrb_dns_header_t *hdr = NULL;
    // TODO: assertion and validation
    // mrb_assert(ret == NULL);

    hdr = (mrb_dns_header_t *)malloc(sizeof(mrb_dns_header_t));
    if (mrb_dns_codec_get_uint16be(mrb, getter, &hdr->id)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "header(id) get failure");
        return -1;
    }

    // a octet as (QR | OPCODE | AA | TC |RD)
    if (mrb_dns_codec_get_uint8(mrb, getter, &w1)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "header(QR|OPCODE|AA|TC|RD) get failure");
        return -1;
    };
    if (w1 & 0x80)
        hdr->qr = 1;
    switch (w1 & 0x78) {
    case 0x10:
        hdr->opcode = 2;
        break;
    case 0x08:
        hdr->opcode = 1;
        break;
    case 0x00:
        hdr->opcode = 0;
        break;
    default:
        mrb_raise(mrb, E_RUNTIME_ERROR, "unkown opcode");
        break;
    }
    if (w1 & 0x04)
        hdr->aa = 1;
    if (w1 & 0x02)
        hdr->tc = 1;
    if (w1 & 0x01)
        hdr->rd = 1;

    // a octet as (RA| Z|AD|CD| RCODE)
    if (mrb_dns_codec_get_uint8(mrb, getter, &w2)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "header(RA|Z|RCODE) get failure");
        return -1;
    }
    if (w2 & 0x80)
        hdr->ra = 1;
    hdr->z      = 0;
    hdr->ad     = 0;
    hdr->cd     = 0;
    hdr->rcode  = w2 & 0x0f;

    if (mrb_dns_codec_get_uint16be(mrb, getter, &hdr->qdcount)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "header(QDCOUNT) get failure");
        return -1;
    }

    if (mrb_dns_codec_get_uint16be(mrb, getter, &hdr->ancount)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "header(ANCOUNT) get failure");
        return -1;
    }
    if (mrb_dns_codec_get_uint16be(mrb, getter, &hdr->nscount)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "header(NSCOUNT) get failure");
        return -1;
    }
    if (mrb_dns_codec_get_uint16be(mrb, getter, &hdr->arcount)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "header(ARCOUNT) get failure");
        return -1;
    }

    ret = hdr;
    return 0;
}

static int mrb_dns_name_append(mrb_state *mrb, mrb_dns_name_t *name, char *node, size_t len) {
    char *buff = NULL;
    size_t l   = 0;
    if (!name) {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "mrb_dns_name_append: name");
        return -1;
    }
    if (!node) {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "mrb_dns_name_append: node");
        return -1;
    }
    if (!name->name) {
        buff = (char *)mrb_malloc(mrb, sizeof(char) * len);
        l    = len;

        memcpy(buff, node, len);
        return 0;
    } else {
        buff = (char *)mrb_malloc(mrb, sizeof(char) * l);
        l    = name->len + 1 + len;

        memcpy(buff, name->name, name->len);
        buff[name->len] = '.';
        memcpy(buff + name->len + 1, node, len);
        mrb_free(mrb, name->name);
    }

    name->name = buff;
    name->len  = l;
    return 0;
}

int mrb_dns_codec_get_name(mrb_state *mrb, mrb_dns_get_state *getter, mrb_dns_name_t *ret) {
    mrb_dns_name_t *name = NULL;
    char *node           = NULL;
    uint8_t len          = 0;
    name                 = NULL;
    // TODO: compression-aware
    mrb_assert(ret == NULL);

    name       = (mrb_dns_name_t *)mrb_malloc(mrb, sizeof(mrb_dns_name_t));
    name->name = NULL;
    name->len  = 0;

    for (mrb_dns_codec_get_uint8(mrb, getter, &len); len > 0;
         mrb_dns_codec_get_uint8(mrb, getter, &len)) {
        if (0xC0 & len) {
            // size_t pos = 0;
            // char *node = NULL;

            mrb_raise(mrb, E_NOTIMP_ERROR, "mrb_dns_codec_get_name: compression");
            break;
        }
        if (mrb_dns_codec_get_str(mrb, getter, len, node)) {
            return -1;
        }
        if(!node){
            mrb_raise(mrb, E_RUNTIME_ERROR, "mrb_dns_codec_get_name: return buffe is null");
        }
        if (mrb_dns_name_append(mrb, name, node, len)) {
            return -1;
        }
    }
    return 0;
}

int mrb_dns_codec_get_question(mrb_state *mrb, mrb_dns_get_state *getter,
                               mrb_dns_question_t *question) {
    mrb_dns_question_t *q = NULL;
    mrb_assert(question == NULL);
    q         = (mrb_dns_question_t *)mrb_malloc(mrb, sizeof(mrb_dns_question_t));
    q->qname  = NULL;
    q->qtype  = 0;
    q->qklass = 0;
    if (mrb_dns_codec_get_name(mrb, getter, q->qname)) {
        return -1;
    }
    if (mrb_dns_codec_get_uint16be(mrb, getter, &q->qtype)) {
        return -1;
    }
    if (mrb_dns_codec_get_uint16be(mrb, getter, &q->qklass)) {
        return -1;
    }

    question = q;
    return 0;
}

int mrb_dns_codec_get_rdata(mrb_state *mrb, mrb_dns_get_state *getter, mrb_dns_rdata_t *ret) {
    mrb_dns_rdata_t *rdata = NULL;
    if (ret != NULL)
        return -1;

    rdata = (mrb_dns_rdata_t *)malloc(sizeof(mrb_dns_rdata_t));

    mrb_dns_codec_get_name(mrb, getter, rdata->name);
    mrb_dns_codec_get_uint16be(mrb, getter, &rdata->typ);
    mrb_dns_codec_get_uint16be(mrb, getter, &rdata->klass);
    mrb_dns_codec_get_uint16be(mrb, getter, &rdata->ttl);
    mrb_dns_codec_get_uint16be(mrb, getter, &rdata->rlength);
    mrb_dns_codec_get_str(mrb, getter, rdata->rlength, (char *)rdata->rdata);
    ret = rdata;
    return 0;
}

int mrb_dns_codec_get(mrb_state *mrb, mrb_dns_get_state *getter, mrb_dns_pkt_t *ret) {
    mrb_dns_pkt_t *pkt = NULL;
    // TODO: assertion
    if (ret != NULL) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "implementation error");
        return -1;
    }
    pkt = (mrb_dns_pkt_t *)mrb_malloc(mrb, sizeof(mrb_dns_pkt_t));

    if (mrb_dns_codec_get_header(mrb, getter, pkt->header)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "header codec failure");
        return -1;
    };

    pkt->questions =
        (mrb_dns_question_t **)mrb_malloc(mrb, sizeof(mrb_dns_question_t *) * pkt->header->qdcount);
    pkt->answers =
        (mrb_dns_rdata_t **)mrb_malloc(mrb, sizeof(mrb_dns_rdata_t *) * pkt->header->ancount);
    pkt->authorities =
        (mrb_dns_rdata_t **)mrb_malloc(mrb, sizeof(mrb_dns_rdata_t *) * pkt->header->nscount);
    pkt->additionals =
        (mrb_dns_rdata_t **)mrb_malloc(mrb, sizeof(mrb_dns_rdata_t *) * pkt->header->arcount);

    for (int i = 0; i < pkt->header->qdcount; i++)
        mrb_dns_codec_get_question(mrb, getter, pkt->questions[i]);

    for (int i = 0; i < pkt->header->ancount; i++)
        mrb_dns_codec_get_rdata(mrb, getter, pkt->answers[i]);

    for (int i = 0; i < pkt->header->nscount; i++)
        mrb_dns_codec_get_rdata(mrb, getter, pkt->authorities[i]);

    for (int i = 0; i < pkt->header->arcount; i++)
        mrb_dns_codec_get_rdata(mrb, getter, pkt->additionals[i]);

    ret = pkt;

    return 0;
}
