#pragma once

#include <lp/msg.h>

#include <memory.h>

extern void msg_allocator_init(void);
extern void msg_allocator_fini(void);

extern lp_msg* msg_allocator_alloc	(unsigned payload_size);
extern void msg_allocator_free		(lp_msg *msg);
extern void msg_allocator_free_at_gvt	(lp_msg *msg);
extern void msg_allocator_fossil_collect(void);

inline lp_msg* msg_allocator_pack(unsigned receiver, simtime_t timestamp,
	unsigned event_type, const void *payload, unsigned payload_size)
{
	lp_msg *msg = msg_allocator_alloc(payload_size);

	msg->dest = receiver;
	msg->dest_t = timestamp;
	msg->m_type = event_type;
	msg->pl_size = payload_size;
#ifndef NEUROME_SERIAL
	atomic_init(&msg->flags, 0U);
#endif
	if(payload_size)
		memcpy(msg->pl, payload, payload_size);
	return msg;
}
