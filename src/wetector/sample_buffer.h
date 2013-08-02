#ifndef SAMPLE_BUFFER_H
#define SAMPLE_BUFFER_H

#include <inttypes.h>
#include <stdio.h>

struct sample_buffer {
  uint16_t size;
  uint16_t start;
  uint16_t count;
  uint32_t sum;
  uint8_t* samples;
};

void sample_buffer_init(struct sample_buffer* buffer, uint8_t* samples, const uint16_t size);

void push_sample(struct sample_buffer* buffer, const uint8_t sample);

void eeprom_read_buffer(struct sample_buffer* buffer, uint16_t pos);

void eeprom_write_buffer(struct sample_buffer* buffer, uint16_t pos);

void print_sample_buffer(struct sample_buffer* buffer, FILE* stream);

#endif
