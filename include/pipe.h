#ifndef MYOS_PIPE_H
#define MYOS_PIPE_H

#include <stdint.h>

#define PIPE_MAX_CHANNELS 4U
#define PIPE_CAPACITY UINT64_C(256)
#define PIPE_INVALID_ID UINT64_MAX

void pipe_init(void);
uint64_t pipe_create(uint64_t owner_task_id);
int pipe_can_attach_writer(uint64_t owner_task_id, uint64_t pipe_id);
int pipe_can_attach_reader(uint64_t owner_task_id, uint64_t pipe_id);
int pipe_attach_writer(uint64_t owner_task_id, uint64_t pipe_id, uint64_t task_id);
int pipe_attach_reader(uint64_t owner_task_id, uint64_t pipe_id, uint64_t task_id);
int pipe_seal(uint64_t owner_task_id, uint64_t pipe_id);
uint64_t pipe_write(uint64_t task_id, const uint8_t *data, uint64_t length);
uint64_t pipe_read(uint64_t task_id, uint8_t *data, uint64_t capacity);
void pipe_release_task(uint64_t task_id);

#endif
