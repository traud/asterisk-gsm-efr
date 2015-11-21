/*** MODULEINFO
	 <depend>amr_nb</depend>
***/

#include "asterisk.h"

/* version 1.0 */
/* based on codecs/codec_opus.c */

#include "asterisk/codec.h"             /* for AST_MEDIA_TYPE_AUDIO */
#include "asterisk/frame.h"             /* for ast_frame, etc */
#include "asterisk/linkedlists.h"       /* for AST_LIST_NEXT, etc */
#include "asterisk/logger.h"            /* for ast_log, ast_debug, etc */
#include "asterisk/module.h"
#include "asterisk/translate.h"         /* for ast_trans_pvt, etc */

#include <opencore-amrnb/interf_dec.h>
#include <opencore-amrnb/interf_enc.h>

#define BUFFER_SAMPLES 8000 /* 1000 milliseconds */

/* Sample frame data */
#include "asterisk/slin.h"
#include "ex_gsm_efr.h"

struct efr_coder_pvt {
	void *state; /* May be encoder or decoder */
	int16_t buf[BUFFER_SAMPLES];
};

static int lintoefr_new(struct ast_trans_pvt *pvt)
{
	struct efr_coder_pvt *apvt = pvt->pvt;
	const int dtx = 0;

	apvt->state = Encoder_Interface_init(dtx);
	
	if (NULL == apvt->state) {
		ast_log(LOG_ERROR, "Error creating the GSM-EFR encoder\n");
		return -1;
	}

	ast_debug(3, "Created encoder (GSM-EFR)\n");

	return 0;
}

static int efrtolin_new(struct ast_trans_pvt *pvt)
{
	struct efr_coder_pvt *apvt = pvt->pvt;

	apvt->state = Decoder_Interface_init();
	
	if (NULL == apvt->state) {
		ast_log(LOG_ERROR, "Error creating the GSM-EFR decoder\n");
		return -1;
	}

	ast_debug(3, "Created decoder (GSM-EFR)\n");

	return 0;
}

static int lintoefr_framein(struct ast_trans_pvt *pvt, struct ast_frame *f)
{
	struct efr_coder_pvt *apvt = pvt->pvt;

	/* XXX We should look at how old the rest of our stream is, and if it
	   is too old, then we should overwrite it entirely, otherwise we can
	   get artifacts of earlier talk that do not belong */
	memcpy(apvt->buf + pvt->samples, f->data.ptr, f->datalen);
	pvt->samples += f->samples;

	return 0;
}

static struct ast_frame *lintoefr_frameout(struct ast_trans_pvt *pvt)
{
	struct efr_coder_pvt *apvt = pvt->pvt;
	const unsigned int frame_size = pvt->t->src_codec.sample_rate / 50;
	struct ast_frame *result = NULL;
	struct ast_frame *last = NULL;
	int samples = 0; /* output samples */

	while (pvt->samples >= frame_size) {
		const int forceSpeech = 0; /* ignored by underlying API anyway */
		unsigned char* out = pvt->outbuf.uc;
		const short* speech = apvt->buf + samples;
		int status; /* result value; either error or output bytes */

		status = Encoder_Interface_Encode(apvt->state, MR122, speech, out, forceSpeech);

		samples += frame_size;
		pvt->samples -= frame_size;

		if (status != 32) {
			ast_log(LOG_ERROR, "Error encoding the GSM-EFR frame\n");
		} else {
			struct ast_frame *current;
			int i;

			out[0] = (0xc0) | (out[1] >> 4);
			for (i = 1, status = status - 1; i < status; i = i + 1) {
				out[i] = (out[i] << 4) | (out[i + 1] >> 4);
			}

			current = ast_trans_frameout(pvt, status, frame_size);

			if (!current) {
				continue;
			} else if (last) {
				AST_LIST_NEXT(last, frame_list) = current;
			} else {
				result = current;
			}
			last = current;
		}
	}
	
	/* Move the data at the end of the buffer to the front */
	if (samples) {
		memmove(apvt->buf, apvt->buf + samples, pvt->samples * 2);
	}

	return result;
}

static int efrtolin_framein(struct ast_trans_pvt *pvt, struct ast_frame *f)
{
	struct efr_coder_pvt *apvt = pvt->pvt;
	const unsigned int frame_size = pvt->t->dst_codec.sample_rate / 50;
	const int bfi = 0; /* ignored by underlying API anyway */
	const unsigned char* data = f->data.ptr;
	unsigned char in[32];
	int i;

	/*
	 * Decoders expect the "MIME storage format" (RFC 4867 chapter 5) which is
	 * octet aligned. GSM-EFR is in the bandwidth-efficient mode.
	 */

	for (i = sizeof(in) / sizeof(in[0]) - 1; i >= 1; i = i - 1) {
		in[i] = (data[i] >> 4) | (data[i - 1] << 4);
	}
	in[0] = 7 << 3; /* AMR mode 7 = GSM-EFR */

	Decoder_Interface_Decode(apvt->state, in, pvt->outbuf.i16 + pvt->datalen, bfi);

	pvt->samples += frame_size;
	pvt->datalen += frame_size * 2;

	return 0;
}

static void lintoefr_destroy(struct ast_trans_pvt *pvt)
{
	struct efr_coder_pvt *apvt = pvt->pvt;

	if (!apvt || !apvt->state) {
		return;
	}

	Encoder_Interface_exit(apvt->state);
	apvt->state = NULL;

	ast_debug(3, "Destroyed encoder (GSM-EFR)\n");
}

static void efrtolin_destroy(struct ast_trans_pvt *pvt)
{
	struct efr_coder_pvt *apvt = pvt->pvt;

	if (!apvt || !apvt->state) {
		return;
	}

	Decoder_Interface_exit(apvt->state);
	apvt->state = NULL;

	ast_debug(3, "Destroyed decoder (GSM-EFR)\n");
}

static struct ast_translator efrtolin = {
        .name = "efrtolin",
        .src_codec = {
                .name = "gsm_efr",
                .type = AST_MEDIA_TYPE_AUDIO,
                .sample_rate = 8000,
        },
        .dst_codec = {
                .name = "slin",
                .type = AST_MEDIA_TYPE_AUDIO,
                .sample_rate = 8000,
        },
        .format = "slin",
        .newpvt = efrtolin_new,
        .framein = efrtolin_framein,
        .destroy = efrtolin_destroy,
        .sample = gsm_efr_sample,
        .desc_size = sizeof(struct efr_coder_pvt),
        .buffer_samples = BUFFER_SAMPLES / 2,
        .buf_size = BUFFER_SAMPLES,
};

static struct ast_translator lintoefr = {
        .name = "lintoefr",
        .src_codec = {
                .name = "slin",
                .type = AST_MEDIA_TYPE_AUDIO,
                .sample_rate = 8000,
        },
        .dst_codec = {
                .name = "gsm_efr",
                .type = AST_MEDIA_TYPE_AUDIO,
                .sample_rate = 8000,
        },
        .format = "gsm_efr",
        .newpvt = lintoefr_new,
        .framein = lintoefr_framein,
        .frameout = lintoefr_frameout,
        .destroy = lintoefr_destroy,
        .sample = slin8_sample,
        .desc_size = sizeof(struct efr_coder_pvt),
        .buffer_samples = BUFFER_SAMPLES / 2,
        .buf_size = BUFFER_SAMPLES,
};

static int unload_module(void)
{
	int res;

	res = ast_unregister_translator(&efrtolin);
	res |= ast_unregister_translator(&lintoefr);

	return res;
}

static int load_module(void)
{
	int res;

	res = ast_register_translator(&efrtolin);
	res |= ast_register_translator(&lintoefr);
	
	if (res) {
		unload_module();
		return AST_MODULE_LOAD_FAILURE;
	}
	
	return AST_MODULE_LOAD_SUCCESS;
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "GSM-EFR Coder/Decoder");
