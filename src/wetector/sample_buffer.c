
#include <inttypes.h>
#include <stdio.h>
#include <avr/eeprom.h>

#include "sample_buffer.h"

void sample_buffer_init(struct sample_buffer* buffer, uint8_t* samples, const uint16_t size) {
    buffer->size = size;
    buffer->start = 0;
    buffer->count = 0;
    buffer->sum = 0;
    buffer->samples = samples;
}
 
uint8_t sample_at(struct sample_buffer* buffer, uint16_t pos) {
  uint16_t real_pos = (buffer->start + pos) % buffer->size;
  return buffer->samples[real_pos];
}

void push_sample(struct sample_buffer* buffer, const uint8_t sample) {
  if (buffer->count == buffer->size) {
    buffer->sum -= buffer->samples[buffer->start];
  }

  uint16_t end = (buffer->start + buffer->count) % buffer->size;
  buffer->samples[end] = sample;
  buffer->sum += sample;

  if (buffer->count == buffer->size) {
    buffer->start = (buffer->start + 1) % buffer->size;      
  } else {
    buffer->count++;
  }
}

void eeprom_read_buffer(struct sample_buffer* buffer, uint16_t pos) {
  eeprom_busy_wait();
  uint16_t length = eeprom_read_word((uint16_t*) pos);
  pos += 2;
  
  for (uint16_t i = 0; i < length; i++) {
    eeprom_busy_wait();  
    push_sample(buffer, eeprom_read_byte((uint8_t*) pos + i));
  }
}

void eeprom_write_buffer(struct sample_buffer* buffer, uint16_t pos) {
  eeprom_busy_wait();
  eeprom_update_word((uint16_t*) pos, buffer->count);
  pos += 2;
  
  for (uint16_t i = 0; i < buffer->count; i++) {
    eeprom_busy_wait();  
    eeprom_update_byte((uint8_t*) pos + i, sample_at(buffer, i));
  }
}

void print_sample_buffer(struct sample_buffer* buffer, FILE* stream) {
  for (uint16_t i = 0; i < buffer->count; i++) {
    fprintf(stream, "%02u", sample_at(buffer, i));
    if (i + 1 < buffer->count) {
      fputc(' ', stream);      
    }
    if ((i + 1) % 25 == 0) {
      fputc('\n', stream);
    }    
  }
  fputc('\n', stream);
  fputc('\n', stream);
}

