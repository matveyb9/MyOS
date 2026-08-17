#include <stdint.h>

#include <pipe.h>

struct pipe_channel {
    int used;
    int sealed;
    uint64_t owner_task_id;
    uint64_t writer_task_id;
    uint64_t reader_task_id;
    uint64_t read_offset;
    uint64_t write_offset;
    uint64_t length;
    uint8_t data[PIPE_CAPACITY];
};

static struct pipe_channel channels[PIPE_MAX_CHANNELS];

static void clear_channel(struct pipe_channel *channel) {
    channel->used = 0;
    channel->sealed = 0;
    channel->owner_task_id = PIPE_INVALID_ID;
    channel->writer_task_id = PIPE_INVALID_ID;
    channel->reader_task_id = PIPE_INVALID_ID;
    channel->read_offset = 0U;
    channel->write_offset = 0U;
    channel->length = 0U;
}

static void release_if_unused(struct pipe_channel *channel) {
    if (channel->used != 0 && channel->owner_task_id == PIPE_INVALID_ID
        && channel->writer_task_id == PIPE_INVALID_ID && channel->reader_task_id == PIPE_INVALID_ID) {
        clear_channel(channel);
    }
}

void pipe_init(void) {
    for (uint64_t index = 0U; index < PIPE_MAX_CHANNELS; index++) {
        clear_channel(&channels[index]);
    }
}

uint64_t pipe_create(uint64_t owner_task_id) {
    if (owner_task_id == 0U) {
        return PIPE_INVALID_ID;
    }
    for (uint64_t index = 0U; index < PIPE_MAX_CHANNELS; index++) {
        struct pipe_channel *channel = &channels[index];

        if (channel->used == 0) {
            channel->used = 1;
            channel->owner_task_id = owner_task_id;
            return index;
        }
    }
    return PIPE_INVALID_ID;
}

int pipe_can_attach_writer(uint64_t owner_task_id, uint64_t pipe_id) {
    const struct pipe_channel *channel;

    if (pipe_id >= PIPE_MAX_CHANNELS) {
        return 0;
    }
    channel = &channels[pipe_id];
    return channel->used != 0 && channel->sealed == 0 && channel->owner_task_id == owner_task_id
           && channel->writer_task_id == PIPE_INVALID_ID;
}

int pipe_can_attach_reader(uint64_t owner_task_id, uint64_t pipe_id) {
    const struct pipe_channel *channel;

    if (pipe_id >= PIPE_MAX_CHANNELS) {
        return 0;
    }
    channel = &channels[pipe_id];
    return channel->used != 0 && channel->sealed == 0 && channel->owner_task_id == owner_task_id
           && channel->reader_task_id == PIPE_INVALID_ID;
}

int pipe_attach_writer(uint64_t owner_task_id, uint64_t pipe_id, uint64_t task_id) {
    if (task_id == 0U || pipe_can_attach_writer(owner_task_id, pipe_id) == 0) {
        return 0;
    }
    channels[pipe_id].writer_task_id = task_id;
    return 1;
}

int pipe_attach_reader(uint64_t owner_task_id, uint64_t pipe_id, uint64_t task_id) {
    if (task_id == 0U || pipe_can_attach_reader(owner_task_id, pipe_id) == 0) {
        return 0;
    }
    channels[pipe_id].reader_task_id = task_id;
    return 1;
}

int pipe_seal(uint64_t owner_task_id, uint64_t pipe_id) {
    struct pipe_channel *channel;

    if (pipe_id >= PIPE_MAX_CHANNELS) {
        return 0;
    }
    channel = &channels[pipe_id];
    if (channel->used == 0 || channel->owner_task_id != owner_task_id) {
        return 0;
    }
    channel->sealed = 1;
    channel->owner_task_id = PIPE_INVALID_ID;
    release_if_unused(channel);
    return 1;
}

uint64_t pipe_write(uint64_t task_id, const uint8_t *data, uint64_t length) {
    struct pipe_channel *channel;

    if (data == (const uint8_t *)0 || length == 0U || length > PIPE_CAPACITY) {
        return PIPE_INVALID_ID;
    }
    for (uint64_t index = 0U; index < PIPE_MAX_CHANNELS; index++) {
        channel = &channels[index];
        if (channel->used != 0 && channel->writer_task_id == task_id) {
            if (PIPE_CAPACITY - channel->length < length) {
                return PIPE_INVALID_ID;
            }
            for (uint64_t copy = 0U; copy < length; copy++) {
                channel->data[channel->write_offset] = data[copy];
                channel->write_offset = (channel->write_offset + 1U) % PIPE_CAPACITY;
            }
            channel->length += length;
            return length;
        }
    }
    return PIPE_INVALID_ID;
}

uint64_t pipe_read(uint64_t task_id, uint8_t *data, uint64_t capacity) {
    struct pipe_channel *channel;
    uint64_t count;

    if (data == (uint8_t *)0 || capacity == 0U) {
        return PIPE_INVALID_ID;
    }
    for (uint64_t index = 0U; index < PIPE_MAX_CHANNELS; index++) {
        channel = &channels[index];
        if (channel->used != 0 && channel->reader_task_id == task_id) {
            if (channel->length == 0U) {
                return channel->writer_task_id == PIPE_INVALID_ID ? 0U : PIPE_INVALID_ID;
            }
            count = channel->length < capacity ? channel->length : capacity;
            for (uint64_t copy = 0U; copy < count; copy++) {
                data[copy] = channel->data[channel->read_offset];
                channel->read_offset = (channel->read_offset + 1U) % PIPE_CAPACITY;
            }
            channel->length -= count;
            return count;
        }
    }
    return PIPE_INVALID_ID;
}

void pipe_release_task(uint64_t task_id) {
    for (uint64_t index = 0U; index < PIPE_MAX_CHANNELS; index++) {
        struct pipe_channel *channel = &channels[index];

        if (channel->used == 0) {
            continue;
        }
        if (channel->owner_task_id == task_id) {
            channel->owner_task_id = PIPE_INVALID_ID;
            channel->sealed = 1;
        }
        if (channel->writer_task_id == task_id) {
            channel->writer_task_id = PIPE_INVALID_ID;
        }
        if (channel->reader_task_id == task_id) {
            channel->reader_task_id = PIPE_INVALID_ID;
        }
        release_if_unused(channel);
    }
}
