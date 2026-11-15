/* SPDX-License-Identifier: BSD-3-Clause */
#define LOG_TAG "AGM: graph_native"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <cutils/properties.h>
#include <sound/asound.h>
#include <sound/compress_offload.h>
#include <sound/compress_params.h>
#include "gsl_intf.h"
#include <agm/graph.h>
#include <agm/graph_module.h>
#include <agm/graph_native.h>
#include <agm/utils.h>

#define NATIVE_CARD_PROPERTY "ro.vendor.audio.native_topology.alias"

int graph_native_card_name(char *name, size_t size)
{
    char value[PROPERTY_VALUE_MAX] = {0};
    int length = property_get(NATIVE_CARD_PROPERTY, value, "");

    if (length <= 0 || (size_t)length >= size)
        return 0;

    memcpy(name, value, length + 1);

    return length;
}

bool graph_native_configures(uint32_t tag)
{
    switch (tag) {
    case DEVICE_HW_ENDPOINT_RX:
    case DEVICE_HW_ENDPOINT_TX:
    case STREAM_INPUT_MEDIA_FORMAT:
    case RD_SHMEM_ENDPOINT:
    case STREAM_PCM_ENCODER:
    case STREAM_PCM_DECODER:
    case STREAM_PCM_CONVERTER:
    case TAG_STREAM_PLACEHOLDER_ENCODER:
    case TAG_STREAM_PLACEHOLDER_DECODER:
    case MODULE_GAPLESS:
        return true;
    default:
        return false;
    }
}

static void sample_bits(enum agm_media_format format, uint32_t *width,
                        uint32_t *bits)
{
    switch (format) {
    case AGM_FORMAT_PCM_S24_LE:
        *width = 24;
        *bits = 32;
        break;
    case AGM_FORMAT_PCM_S24_3LE:
        *width = 24;
        *bits = 24;
        break;
    case AGM_FORMAT_PCM_S32_LE:
        *width = 32;
        *bits = 32;
        break;
    default:
        *width = 16;
        *bits = 16;
        break;
    }
}

static int sample_format(unsigned bits, uint32_t *format)
{
    switch (bits) {
    case 0:
        *format = 0;
        return 0;
    case 16:
        *format = SNDRV_PCM_FORMAT_S16_LE;
        return 0;
    case 24:
        *format = SNDRV_PCM_FORMAT_S24_LE;
        return 0;
    case 32:
        *format = SNDRV_PCM_FORMAT_S32_LE;
        return 0;
    default:
        return -EINVAL;
    }
}

static int aac(const struct agm_session_aac_dec *input, struct snd_codec *codec)
{
    switch (input->aac_fmt_flag) {
    case 0:
        codec->format = SND_AUDIOSTREAMFORMAT_MP4ADTS;
        break;
    case 1:
        codec->format = SND_AUDIOSTREAMFORMAT_MP4LOAS;
        break;
    case 2:
        codec->format = SND_AUDIOSTREAMFORMAT_ADIF;
        break;
    case 3:
        codec->format = SND_AUDIOSTREAMFORMAT_RAW;
        break;
    case 4:
        codec->format = SND_AUDIOSTREAMFORMAT_MP4LATM;
        break;
    default:
        return -EINVAL;
    }

    switch (input->audio_obj_type) {
    case 2:
        codec->profile = SND_AUDIOMODE_AAC_LC;
        break;
    case 29:
        codec->profile = SND_AUDIOMODE_AAC_HE_PS;
        break;
    default:
        codec->profile = SND_AUDIOMODE_AAC_HE;
        break;
    }

    codec->id = SND_AUDIOCODEC_AAC;
    codec->ch_in = input->num_channels;

    return 0;
}

static int wma_profile(unsigned tag, uint32_t *profile)
{
    switch (tag) {
    case 0x161:
        *profile = SND_AUDIOPROFILE_WMA9;
        return 0;
    case 0x162:
        *profile = SND_AUDIOPROFILE_WMA9_PRO;
        return 0;
    case 0x163:
        *profile = SND_AUDIOPROFILE_WMA9_LOSSLESS;
        return 0;
    case 0x166:
        *profile = SND_AUDIOPROFILE_WMA10;
        return 0;
    case 0x167:
        *profile = SND_AUDIOPROFILE_WMA10_LOSSLESS;
        return 0;
    default:
        return -EINVAL;
    }
}

static int decoder(const struct agm_session_config *config,
                   const struct agm_media_config *media,
                   struct snd_codec *codec)
{
    const union agm_session_codec *input = &config->codec;

    switch (media->format) {
    case AGM_FORMAT_MP3:
        codec->id = SND_AUDIOCODEC_MP3;
        return 0;
    case AGM_FORMAT_AAC:
        return aac(&input->aac_dec, codec);
    case AGM_FORMAT_FLAC:
        codec->id = SND_AUDIOCODEC_FLAC;
        codec->ch_in = input->flac_dec.num_channels;
        codec->options.flac_d.sample_size = input->flac_dec.sample_size;
        codec->options.flac_d.min_blk_size = input->flac_dec.min_blk_size;
        codec->options.flac_d.max_blk_size = input->flac_dec.max_blk_size;
        codec->options.flac_d.min_frame_size = input->flac_dec.min_frame_size;
        codec->options.flac_d.max_frame_size = input->flac_dec.max_frame_size;
        return 0;
    case AGM_FORMAT_ALAC:
        codec->id = SND_AUDIOCODEC_ALAC;
        codec->ch_in = input->alac_dec.num_channels;
        codec->bit_rate = input->alac_dec.avg_bit_rate;
        codec->options.alac_d.frame_length = input->alac_dec.frame_length;
        codec->options.alac_d.compatible_version =
            input->alac_dec.compatible_version;
        codec->options.alac_d.pb = input->alac_dec.pb;
        codec->options.alac_d.mb = input->alac_dec.mb;
        codec->options.alac_d.kb = input->alac_dec.kb;
        codec->options.alac_d.max_run = input->alac_dec.max_run;
        codec->options.alac_d.max_frame_bytes = input->alac_dec.max_frame_bytes;
        return sample_format(input->alac_dec.bit_depth, &codec->pcm_format);
    case AGM_FORMAT_APE:
        codec->id = SND_AUDIOCODEC_APE;
        codec->ch_in = input->ape_dec.num_channels;
        codec->options.ape_d.compatible_version =
            input->ape_dec.compatible_version;
        codec->options.ape_d.compression_level = input->ape_dec.compression_level;
        codec->options.ape_d.format_flags = input->ape_dec.format_flags;
        codec->options.ape_d.blocks_per_frame = input->ape_dec.blocks_per_frame;
        codec->options.ape_d.final_frame_blocks =
            input->ape_dec.final_frame_blocks;
        codec->options.ape_d.total_frames = input->ape_dec.total_frames;
        codec->options.ape_d.seek_table_present =
            input->ape_dec.seek_table_present;
        return sample_format(input->ape_dec.bit_width, &codec->pcm_format);
    case AGM_FORMAT_VORBIS:
        codec->id = SND_AUDIOCODEC_VORBIS;
        return 0;
    case AGM_FORMAT_WMASTD:
        codec->id = SND_AUDIOCODEC_WMA;
        codec->ch_in = input->wma_dec.num_channels;
        codec->bit_rate = input->wma_dec.avg_bytes_per_sec * 8;
        codec->align = input->wma_dec.blk_align;
        codec->options.wma_d.encoder_option = input->wma_dec.enc_options;

        if (input->wma_dec.fmt_tag != 0x161)
            return -EINVAL;

        codec->profile = SND_AUDIOPROFILE_WMA9;

        return sample_format(input->wma_dec.bits_per_sample, &codec->pcm_format);
    case AGM_FORMAT_WMAPRO: {
        codec->id = SND_AUDIOCODEC_WMA;
        codec->ch_in = input->wmapro_dec.num_channels;
        codec->bit_rate = input->wmapro_dec.avg_bytes_per_sec * 8;
        codec->align = input->wmapro_dec.blk_align;
        codec->options.wma_d.encoder_option = input->wmapro_dec.enc_options;
        codec->options.wma_d.adv_encoder_option =
            input->wmapro_dec.advanced_enc_option;
        codec->options.wma_d.adv_encoder_option2 =
            input->wmapro_dec.advanced_enc_option2;

        int status = wma_profile(input->wmapro_dec.fmt_tag, &codec->profile);
        if (status || codec->profile == SND_AUDIOPROFILE_WMA9)
            return -EINVAL;

        return sample_format(input->wmapro_dec.bits_per_sample,
                             &codec->pcm_format);
    }
    case AGM_FORMAT_OPUS:
        codec->id = SND_AUDIOCODEC_OPUS_RAW;
        codec->ch_in = input->opus_dec.num_channels;
        codec->options.opus_d.version = input->opus_dec.version;
        codec->options.opus_d.num_channels = input->opus_dec.num_channels;
        codec->options.opus_d.pre_skip = input->opus_dec.pre_skip;
        codec->options.opus_d.sample_rate = input->opus_dec.sample_rate;
        codec->options.opus_d.output_gain = input->opus_dec.output_gain;
        codec->options.opus_d.mapping_family = input->opus_dec.mapping_family;
        codec->options.opus_d.chan_map.stream_count =
            input->opus_dec.stream_count;
        codec->options.opus_d.chan_map.coupled_count =
            input->opus_dec.coupled_count;
        memcpy(codec->options.opus_d.chan_map.channel_map,
                    input->opus_dec.channel_map,
                    sizeof(input->opus_dec.channel_map));
        return 0;
    default:
        return -EOPNOTSUPP;
    }
}

static int encoder(const struct agm_session_config *config,
                   const struct agm_media_config *media,
                   struct snd_codec *codec)
{
    const struct agm_session_aac_enc *input = &config->codec.aac_enc;

    if (media->format != AGM_FORMAT_AAC)
        return -EOPNOTSUPP;

    codec->id = SND_AUDIOCODEC_AAC;

    switch (input->enc_cfg.aac_enc_mode) {
    case 0x02:
        codec->profile = SND_AUDIOMODE_AAC_LC;
        break;
    case 0x05:
        codec->profile = SND_AUDIOMODE_AAC_HE;
        break;
    case 0x1d:
        codec->profile = SND_AUDIOMODE_AAC_HE_PS;
        break;
    default:
        return -EINVAL;
    }

    switch (input->enc_cfg.aac_fmt_flag) {
    case 0:
        codec->format = SND_AUDIOSTREAMFORMAT_MP4ADTS;
        break;
    case 1:
        codec->format = SND_AUDIOSTREAMFORMAT_MP4LOAS;
        break;
    case 4:
        codec->format = SND_AUDIOSTREAMFORMAT_MP4LATM;
        break;
    default:
        return -EOPNOTSUPP;
    }

    codec->bit_rate = input->aac_bit_rate;
    codec->ch_in = media->channels;

    return 0;
}

static int compress_params(const struct agm_session_config *config,
                           const struct agm_media_config *media,
                           const struct agm_buffer_config *buffer,
                           struct snd_compr_params *params)
{
    struct snd_compr_params result;
    int status;

    if (config->dir != RX && config->dir != TX)
        return -EOPNOTSUPP;

    if (!media->rate || !media->channels || !buffer->size || !buffer->count ||
        buffer->size > UINT32_MAX)
        return -EINVAL;

    memset(&result, 0, sizeof(result));

    status = config->dir == TX ? encoder(config, media, &result.codec) :
                                 decoder(config, media, &result.codec);
    if (status)
        return status;

    if (!result.codec.ch_in)
        result.codec.ch_in = media->channels;

    result.codec.ch_out = media->channels;
    result.codec.sample_rate = media->rate;
    result.buffer.fragment_size = buffer->size;
    result.buffer.fragments = buffer->count;
    *params = result;

    return 0;
}

int graph_native_set_media(gsl_handle_t graph, struct session_obj *session)
{
    const struct agm_session_config *config = &session->stream_config;
    bool capture = config->dir == TX;
    const struct agm_media_config *media = capture ?
        &session->in_media_config : &session->out_media_config;
    const struct agm_buffer_config *buffer = capture ?
        &session->in_buffer_config : &session->out_buffer_config;
    int ret;

    if (config->sess_mode == AGM_SESSION_COMPRESS) {
        struct snd_compr_params params;

        ret = compress_params(config, media, buffer, &params);
        if (ret) {
            AGM_LOGE("no compress parameters for format %d, error %d\n",
                     media->format, ret);
            return ret;
        }

        ret = gsl_ioctl(graph, GSL_CMD_CONFIGURE_COMPRESS_MEDIA, &params,
                        sizeof(params));
    } else {
        struct gsl_cmd_configure_pcm_media pcm = {0};

        pcm.sample_rate = media->rate;
        pcm.num_channels = media->channels;
        sample_bits(media->format, &pcm.bit_width, &pcm.bits_per_sample);

        ret = gsl_ioctl(graph, GSL_CMD_CONFIGURE_PCM_MEDIA, &pcm, sizeof(pcm));
    }

    return ar_err_get_lnx_err_code(ret);
}

int graph_native_set_backend(gsl_handle_t graph,
                             const struct agm_key_vector_gsl *gkv,
                             struct device_obj *device)
{
    struct gsl_cmd_configure_backend_media backend = {0};
    const struct agm_media_config *media = &device->media_config;
    int ret;

    backend.graph_key_vector.num_kvps = gkv->num_kvs;
    backend.graph_key_vector.kvp = (struct gsl_key_value_pair *)gkv->kv;
    backend.backend = device->pcm_id;
    backend.sample_rate = media->rate;
    backend.num_channels = media->channels;
    backend.data_format = media->data_format;
    sample_bits(media->format, &backend.bit_width, &backend.bits_per_sample);

    AGM_LOGD("backend %s (%u): %u Hz, %u channels, format %d\n", device->name,
             device->pcm_id, media->rate, media->channels, media->format);

    ret = gsl_ioctl(graph, GSL_CMD_CONFIGURE_BACKEND_MEDIA, &backend,
                    sizeof(backend));

    return ar_err_get_lnx_err_code(ret);
}
