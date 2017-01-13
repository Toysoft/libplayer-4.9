#ifndef ADEC_ASSOC_AUDIO_H_
#define ADEC_ASSOC_AUDIO_H_

int InAssocBufferInit(aml_audio_dec_t *audec);
int InAssocBufferRelease(aml_audio_dec_t *audec);

void get_assoc_stream_info(buffer_stream_t *bs);
int set_assoc_enable(unsigned int enable);
int get_assoc_status(void);
int read_assoc_data(aml_audio_dec_t *audec, unsigned char *buffer, int size);
void audio_set_mixing_level_between_main_assoc(int mixing_level);

#endif

